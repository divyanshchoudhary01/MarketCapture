#include "marketcapture/threaded_engine.hpp"
#include "marketcapture/book_router.hpp"
#include "marketcapture/itch.hpp"
#include "marketcapture/recorder.hpp"
#include "marketcapture/ring_buffer.hpp"
#include <array>
#include <atomic>
#include <cstring>
#include <exception>
#include <optional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace marketcapture {
namespace {
constexpr std::size_t packet_slots = 1024;
constexpr std::size_t max_datagram_bytes = 65536;
constexpr std::size_t event_slots = 4096;

struct PacketSlot {
    std::array<std::uint8_t, max_datagram_bytes> bytes{};
    std::uint32_t size{};
    FeedChannel channel{};
};
struct EventEnvelope {
    std::uint64_t sequence{};
    Event event;
};
struct MetricEvent {
    std::uint64_t sequence{};
};

template <typename Queue, typename Value>
bool push_wait(Queue& queue, Value value, const std::atomic<bool>& abort) {
    // SpscRingBuffer accepts by value, so moving here would consume the event
    // even when a full queue rejects it. Preserve the source across retries.
    while (!queue.try_push(value)) {
        if (abort.load(std::memory_order_acquire)) return false;
        std::this_thread::yield();
    }
    return true;
}
}

class ThreadedCaptureEngine::Impl {
public:
    Impl(ThreadedEngineConfig input, RecoveryHandler recovery)
        : config(std::move(input)), books(config.book_shards) {
        if (config.book_shards == 0)
            throw std::invalid_argument("threaded engine needs at least one book shard");
        for (std::size_t index = 0; index < packet_slots - 1; ++index)
            if (!free_slots.try_push(index))
                throw std::logic_error("cannot initialize packet pool");
        if (!config.record_path.empty())
            recorder.emplace(config.record_path);

        arbitrator = std::make_unique<FeedArbitrator>(
            [this](const ArbitratedMessage& message) {
                auto event = parser.parse(message.payload);
                EventEnvelope envelope{message.sequence, std::move(event)};
                if (!push_wait(book_queue, envelope, abort_workers)) return;
                if (is_order_tick(envelope.event) &&
                    !push_wait(record_queue, envelope, abort_workers)) return;
                if (!push_wait(metrics_queue, MetricEvent{message.sequence}, abort_workers))
                    return;
                messages_parsed.fetch_add(1, std::memory_order_relaxed);
            },
            [handler = std::move(recovery), this](const RecoveryRequest& request) {
                if (handler) handler(request);
                else gaps_unrecovered.fetch_add(
                    request.last_sequence - request.first_sequence + 1,
                    std::memory_order_relaxed);
            },
            config.initial_sequence, config.max_reorder_messages);

        book_thread = std::thread([this] { guard([this] { book_loop(); }); });
        recorder_thread = std::thread([this] { guard([this] { recorder_loop(); }); });
        metrics_thread = std::thread([this] { guard([this] { metrics_loop(); }); });
        parser_thread = std::thread([this] { guard([this] { parser_loop(); }); });
    }

    ~Impl() { stop(); }

    template <typename Function>
    void guard(Function function) noexcept {
        try { function(); }
        catch (...) {
            {
                std::lock_guard lock(error_mutex);
                if (!worker_error) worker_error = std::current_exception();
            }
            abort_workers.store(true, std::memory_order_release);
            accepting.store(false, std::memory_order_release);
            parser_done.store(true, std::memory_order_release);
        }
    }

    void parser_loop() {
        while (!abort_workers.load(std::memory_order_acquire) &&
               (accepting.load(std::memory_order_acquire) || !ingress.empty())) {
            auto index = ingress.try_pop();
            if (!index) { std::this_thread::yield(); continue; }
            auto& slot = packets[*index];
            try {
                arbitrator->ingest(slot.channel,
                    std::span<const std::uint8_t>(slot.bytes.data(), slot.size));
            } catch (...) {
                parse_errors.fetch_add(1, std::memory_order_relaxed);
                throw;
            }
            while (!free_slots.try_push(*index)) std::this_thread::yield();
            packets_consumed.fetch_add(1, std::memory_order_release);
        }
        parser_done.store(true, std::memory_order_release);
    }

    void book_loop() {
        while (!parser_done.load(std::memory_order_acquire) || !book_queue.empty()) {
            auto item = book_queue.try_pop();
            if (!item) { std::this_thread::yield(); continue; }
            books.apply(item->event);
            book_updates.fetch_add(1, std::memory_order_relaxed);
        }
        active_symbols.store(books.symbol_count(), std::memory_order_relaxed);
        active_orders.store(books.order_count(), std::memory_order_relaxed);
    }

    void recorder_loop() {
        while (!parser_done.load(std::memory_order_acquire) || !record_queue.empty()) {
            auto item = record_queue.try_pop();
            if (!item) { std::this_thread::yield(); continue; }
            if (recorder) recorder->write(item->event);
            recorded_events.fetch_add(1, std::memory_order_relaxed);
        }
        if (recorder) recorder->flush();
    }

    void metrics_loop() {
        while (!parser_done.load(std::memory_order_acquire) || !metrics_queue.empty()) {
            auto item = metrics_queue.try_pop();
            if (!item) { std::this_thread::yield(); continue; }
            last_sequence.store(item->sequence, std::memory_order_relaxed);
            metrics_events.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void stop() {
        if (stopped.exchange(true)) return;
        accepting.store(false, std::memory_order_release);
        if (parser_thread.joinable()) parser_thread.join();
        if (book_thread.joinable()) book_thread.join();
        if (recorder_thread.joinable()) recorder_thread.join();
        if (metrics_thread.joinable()) metrics_thread.join();
        recorder.reset();
    }

    ThreadedEngineConfig config;
    std::array<PacketSlot, packet_slots> packets{};
    SpscRingBuffer<std::size_t, packet_slots> free_slots;
    SpscRingBuffer<std::size_t, packet_slots> ingress;
    SpscRingBuffer<EventEnvelope, event_slots> book_queue;
    SpscRingBuffer<EventEnvelope, event_slots> record_queue;
    SpscRingBuffer<MetricEvent, event_slots> metrics_queue;
    ItchParser parser;
    ShardedBookRouter books;
    std::optional<TickRecorder> recorder;
    std::unique_ptr<FeedArbitrator> arbitrator;
    std::thread parser_thread, book_thread, recorder_thread, metrics_thread;
    std::atomic<bool> accepting{true}, parser_done{false}, abort_workers{false}, stopped{false};
    std::atomic<std::uint64_t> packets_submitted{}, packets_rejected{}, packets_consumed{};
    std::atomic<std::uint64_t> messages_parsed{}, book_updates{}, recorded_events{};
    std::atomic<std::uint64_t> metrics_events{}, parse_errors{}, gaps_unrecovered{};
    std::atomic<std::uint64_t> last_sequence{};
    std::atomic<std::size_t> active_symbols{}, active_orders{};
    mutable std::mutex error_mutex;
    std::exception_ptr worker_error;
};

ThreadedCaptureEngine::ThreadedCaptureEngine(ThreadedEngineConfig config,
                                             RecoveryHandler recovery_handler)
    : impl_(std::make_unique<Impl>(std::move(config), std::move(recovery_handler))) {}
ThreadedCaptureEngine::~ThreadedCaptureEngine() = default;

bool ThreadedCaptureEngine::submit(FeedChannel channel,
                                   std::span<const std::uint8_t> datagram) noexcept {
    if (!impl_->accepting.load(std::memory_order_acquire) ||
        datagram.empty() || datagram.size() > max_datagram_bytes) {
        impl_->packets_rejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    auto index = impl_->free_slots.try_pop();
    if (!index) {
        impl_->packets_rejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    auto& slot = impl_->packets[*index];
    std::memcpy(slot.bytes.data(), datagram.data(), datagram.size());
    slot.size = static_cast<std::uint32_t>(datagram.size());
    slot.channel = channel;
    if (!impl_->ingress.try_push(*index)) {
        while (!impl_->free_slots.try_push(*index)) std::this_thread::yield();
        impl_->packets_rejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    impl_->packets_submitted.fetch_add(1, std::memory_order_release);
    return true;
}

bool ThreadedCaptureEngine::wait_until_idle(std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (impl_->packets_consumed.load(std::memory_order_acquire) ==
                impl_->packets_submitted.load(std::memory_order_acquire) &&
            impl_->ingress.empty() && impl_->book_queue.empty() &&
            impl_->record_queue.empty() && impl_->metrics_queue.empty())
            return true;
        std::this_thread::yield();
    }
    return false;
}

void ThreadedCaptureEngine::stop() {
    impl_->stop();
    rethrow_worker_error();
}

ThreadedEngineStats ThreadedCaptureEngine::stats() const noexcept {
    ThreadedEngineStats result;
    result.packets_submitted = impl_->packets_submitted.load();
    result.packets_rejected = impl_->packets_rejected.load();
    result.messages_parsed = impl_->messages_parsed.load();
    result.book_updates = impl_->book_updates.load();
    result.recorded_events = impl_->recorded_events.load();
    result.metrics_events = impl_->metrics_events.load();
    result.parse_errors = impl_->parse_errors.load();
    result.duplicate_messages = impl_->arbitrator->duplicates();
    result.recovery_requests = impl_->arbitrator->recovery_requests();
    result.active_symbols = impl_->active_symbols.load();
    result.active_orders = impl_->active_orders.load();
    return result;
}

void ThreadedCaptureEngine::rethrow_worker_error() const {
    std::lock_guard lock(impl_->error_mutex);
    if (impl_->worker_error) std::rethrow_exception(impl_->worker_error);
}

} // namespace marketcapture
