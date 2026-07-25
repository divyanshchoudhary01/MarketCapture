#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace marketcapture {

enum class Side : std::uint8_t { buy = 'B', sell = 'S' };

struct SystemEvent {
    std::uint64_t timestamp{};
    char code{};
};

struct StockDirectory {
    std::uint64_t timestamp{};
    std::string symbol;
    char market_category{};
    char financial_status{};
    std::uint32_t round_lot_size{};
    bool round_lots_only{};
    char issue_classification{};
    std::string issue_subtype;
    char authenticity{};
    char short_sale_threshold{};
    char ipo_flag{};
    char luld_reference_tier{};
    char etp_flag{};
    std::uint32_t etp_leverage_factor{};
    bool inverse_indicator{};
};

struct TradingAction {
    std::uint64_t timestamp{};
    std::string symbol;
    char state{};
    std::string reason;
};

struct RegShoRestriction {
    std::uint64_t timestamp{};
    std::string symbol;
    char action{};
};

struct MarketParticipantPosition {
    std::uint64_t timestamp{};
    std::string mpid;
    std::string symbol;
    bool primary_market_maker{};
    char market_maker_mode{};
    char participant_state{};
};

struct MwcbDeclineLevels {
    std::uint64_t timestamp{};
    std::uint64_t level1{};
    std::uint64_t level2{};
    std::uint64_t level3{};
};

struct MwcbStatus {
    std::uint64_t timestamp{};
    char breached_level{};
};

struct IpoQuotingPeriod {
    std::uint64_t timestamp{};
    std::string symbol;
    std::uint32_t release_time{};
    char qualifier{};
    std::uint32_t price{};
};

struct LuldAuctionCollar {
    std::uint64_t timestamp{};
    std::string symbol;
    std::uint32_t reference_price{};
    std::uint32_t lower_price{};
    std::uint32_t upper_price{};
    std::uint32_t extension{};
};

struct OperationalHalt {
    std::uint64_t timestamp{};
    std::string symbol;
    char market_code{};
    char action{};
};

struct AddOrder {
    std::uint64_t timestamp{};
    std::uint64_t order_id{};
    Side side{};
    std::uint32_t shares{};
    std::string symbol;
    std::uint32_t price{}; // ITCH fixed-point price (1/10000 USD)
};

struct AddOrderMpid {
    AddOrder order;
    std::string attribution;
};

struct ExecuteOrder {
    std::uint64_t timestamp{};
    std::uint64_t order_id{};
    std::uint32_t shares{};
    std::uint64_t match_id{};
};

struct ExecuteOrderPrice {
    ExecuteOrder execution;
    bool printable{};
    std::uint32_t price{};
};

struct CancelOrder {
    std::uint64_t timestamp{};
    std::uint64_t order_id{};
    std::uint32_t shares{};
};

struct DeleteOrder {
    std::uint64_t timestamp{};
    std::uint64_t order_id{};
};

struct ReplaceOrder {
    std::uint64_t timestamp{};
    std::uint64_t original_order_id{};
    std::uint64_t new_order_id{};
    std::uint32_t shares{};
    std::uint32_t price{};
};

struct Trade {
    std::uint64_t timestamp{};
    std::uint64_t order_id{};
    Side side{};
    std::uint32_t shares{};
    std::string symbol;
    std::uint32_t price{};
    std::uint64_t match_id{};
};

struct CrossTrade {
    std::uint64_t timestamp{};
    std::uint64_t shares{};
    std::string symbol;
    std::uint32_t price{};
    std::uint64_t match_id{};
    char cross_type{};
};

struct BrokenTrade {
    std::uint64_t timestamp{};
    std::uint64_t match_id{};
};

struct Noii {
    std::uint64_t timestamp{};
    std::uint64_t paired_shares{};
    std::uint64_t imbalance_shares{};
    char direction{};
    std::string symbol;
    std::uint32_t far_price{};
    std::uint32_t near_price{};
    std::uint32_t current_reference_price{};
    char cross_type{};
    char price_variation{};
};

struct RetailPriceImprovement {
    std::uint64_t timestamp{};
    std::string symbol;
    char interest{};
};

using Event = std::variant<
    SystemEvent, StockDirectory, TradingAction, RegShoRestriction,
    MarketParticipantPosition, MwcbDeclineLevels, MwcbStatus, IpoQuotingPeriod,
    LuldAuctionCollar, OperationalHalt, AddOrder, AddOrderMpid, ExecuteOrder,
    ExecuteOrderPrice, CancelOrder, DeleteOrder, ReplaceOrder, Trade, CrossTrade,
    BrokenTrade, Noii, RetailPriceImprovement>;

[[nodiscard]] inline bool is_order_tick(const Event& event) noexcept {
    return std::holds_alternative<AddOrder>(event) ||
           std::holds_alternative<AddOrderMpid>(event) ||
           std::holds_alternative<ExecuteOrder>(event) ||
           std::holds_alternative<ExecuteOrderPrice>(event) ||
           std::holds_alternative<CancelOrder>(event) ||
           std::holds_alternative<DeleteOrder>(event) ||
           std::holds_alternative<ReplaceOrder>(event);
}

} // namespace marketcapture
