from pathlib import Path
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle,
    Image, KeepTogether, HRFlowable
)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output" / "pdf"
OUT.mkdir(parents=True, exist_ok=True)
ARCH = ROOT / "docs" / "images" / "market_data_capture_architecture.png"

NAVY = colors.HexColor("#10243E")
BLUE = colors.HexColor("#1E64A3")
CYAN = colors.HexColor("#DDF4F2")
LIGHT = colors.HexColor("#F3F6FA")
ORANGE = colors.HexColor("#E68A2E")
TEXT = colors.HexColor("#263442")
MUTED = colors.HexColor("#66788A")

styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name="CoverTitle", parent=styles["Title"], fontName="Helvetica-Bold",
                          fontSize=30, leading=35, textColor=NAVY, alignment=TA_CENTER,
                          spaceAfter=10))
styles.add(ParagraphStyle(name="CoverSub", parent=styles["Normal"], fontName="Helvetica",
                          fontSize=13, leading=19, textColor=MUTED, alignment=TA_CENTER))
styles.add(ParagraphStyle(name="H1x", parent=styles["Heading1"], fontName="Helvetica-Bold",
                          fontSize=19, leading=23, textColor=NAVY, spaceBefore=8, spaceAfter=8))
styles.add(ParagraphStyle(name="H2x", parent=styles["Heading2"], fontName="Helvetica-Bold",
                          fontSize=13, leading=17, textColor=BLUE, spaceBefore=8, spaceAfter=5))
styles.add(ParagraphStyle(name="Bodyx", parent=styles["BodyText"], fontName="Helvetica",
                          fontSize=9.4, leading=14, textColor=TEXT, spaceAfter=6))
styles.add(ParagraphStyle(name="Smallx", parent=styles["BodyText"], fontName="Helvetica",
                          fontSize=8, leading=11, textColor=MUTED))
styles.add(ParagraphStyle(name="Callout", parent=styles["BodyText"], fontName="Helvetica-Bold",
                          fontSize=9.3, leading=14, textColor=NAVY, backColor=CYAN,
                          borderPadding=8, spaceBefore=5, spaceAfter=8))
styles.add(ParagraphStyle(name="CodeX", parent=styles["Code"], fontName="Courier",
                          fontSize=7.5, leading=10, backColor=LIGHT, borderPadding=7,
                          spaceBefore=4, spaceAfter=7))

def header_footer(canvas, doc):
    canvas.saveState()
    canvas.setStrokeColor(colors.HexColor("#D7E0EA"))
    canvas.line(18*mm, 15*mm, 192*mm, 15*mm)
    canvas.setFont("Helvetica", 7.5)
    canvas.setFillColor(MUTED)
    canvas.drawString(18*mm, 10*mm, doc.title)
    canvas.drawRightString(192*mm, 10*mm, f"MarketCapture v2.0.0  |  {doc.page}")
    canvas.restoreState()

def cover(title, subtitle, audience):
    story = [Spacer(1, 30*mm)]
    story += [Paragraph("MARKETCAPTURE", styles["CoverSub"]), Spacer(1, 8*mm)]
    story += [Paragraph(title, styles["CoverTitle"]), Paragraph(subtitle, styles["CoverSub"])]
    story += [Spacer(1, 18*mm), HRFlowable(width="75%", color=BLUE, thickness=2)]
    story += [Spacer(1, 12*mm), Paragraph(audience, styles["CoverSub"])]
    story += [Spacer(1, 40*mm), Paragraph(
        "C++20 | NASDAQ ITCH 5.0 | MoldUDP64 | deterministic recovery | bounded-memory hot path",
        styles["Smallx"])]
    story += [PageBreak()]
    return story

def p(text, style="Bodyx"):
    return Paragraph(text, styles[style])

def bullets(items):
    return [Paragraph(f"- {item}", styles["Bodyx"]) for item in items]

def table(rows, widths=None):
    t = Table([[Paragraph(str(cell), styles["Smallx"]) for cell in row] for row in rows],
              colWidths=widths, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0,0), (-1,0), NAVY), ("TEXTCOLOR", (0,0), (-1,0), colors.white),
        ("FONTNAME", (0,0), (-1,0), "Helvetica-Bold"),
        ("GRID", (0,0), (-1,-1), 0.35, colors.HexColor("#CBD5E1")),
        ("VALIGN", (0,0), (-1,-1), "TOP"), ("ROWBACKGROUNDS", (0,1), (-1,-1), [colors.white, LIGHT]),
        ("LEFTPADDING", (0,0), (-1,-1), 5), ("RIGHTPADDING", (0,0), (-1,-1), 5),
        ("TOPPADDING", (0,0), (-1,-1), 5), ("BOTTOMPADDING", (0,0), (-1,-1), 5),
    ]))
    return t

def section(story, title, paragraphs, sub=None):
    story.append(p(title, "H1x"))
    if sub: story.append(p(sub, "Callout"))
    for text in paragraphs: story.append(p(text))

def build_pdf(filename, title, story):
    doc = SimpleDocTemplate(str(OUT / filename), pagesize=A4, title=title,
                            rightMargin=18*mm, leftMargin=18*mm,
                            topMargin=17*mm, bottomMargin=20*mm)
    doc.build(story, onFirstPage=header_footer, onLaterPages=header_footer)

flow = cover("End-to-End System Flow", "How one market-data packet becomes a deterministic order book and durable evidence",
             "A visual operational guide for developers, reviewers, and interviewers")
flow += [p("Reading map", "H1x"), table([
    ["Stage", "Purpose", "Thread / ownership"],
    ["NIC receive", "Accept UDP/DPDK/io_uring input", "Single RX producer"],
    ["Arbitration", "Merge A/B, suppress duplicates, block gaps", "Parser worker"],
    ["ITCH parsing", "Validate binary messages and normalize events", "Parser worker"],
    ["Fan-out", "Publish bounded values to independent consumers", "SPSC queues"],
    ["Book", "Maintain order and price-level state", "Arena-book worker"],
    ["Recorder / PCAP", "Persist normalized events and raw packets", "Dedicated workers"],
    ["Metrics", "Count progress, errors, rejects, and queue pressure", "Metrics worker"],
], [31*mm, 92*mm, 48*mm]), PageBreak()]
if ARCH.exists():
    flow += [p("System architecture", "H1x"), Image(str(ARCH), width=174*mm, height=130*mm),
             p("The original architecture is extended in v2 with fixed-width hot events, bounded arbitration, arena-backed books, asynchronous PCAP, and explicit overload telemetry.", "Smallx"),
             PageBreak()]
section(flow, "1. Receive and timestamp", [
    "The live receiver joins a provider-assigned multicast group or accepts frames through DPDK. The submitter performs bounded work: optional timestamp acquisition, one copy into a preallocated packet slot, and publication of the slot index.",
    "A rejected submit means the fixed pool is exhausted or shutdown has started. The reject policy protects the RX thread; the spin policy applies backpressure when the deployment explicitly chooses it."
], "Hot-path rule: no filesystem calls, console output, compression, or book mutation occurs in the NIC callback.")
flow += [p("Packet lifecycle", "H2x"), p("NIC -> fixed packet slot -> ingress SPSC -> parser consumes -> slot returned to free pool", "CodeX")]
section(flow, "2. Decode MoldUDP64", [
    "The decoder validates the 20-byte MoldUDP64 header, reads session, first sequence, and message count, then creates non-owning spans for length-prefixed ITCH messages.",
    "Malformed lengths, truncation, and trailing bytes are rejected. Spans remain valid only while the packet slot is owned by the parser stage."
])
section(flow, "3. A/B arbitration and recovery", [
    "Channel A and B carry redundant sequence streams. A bounded circular reorder window stores fixed 64-byte ITCH payloads. A duplicate sequence is dropped. A future sequence blocks delivery and raises a missing-range request.",
    "Recovered packets re-enter through the recovery channel. Delivery resumes only in exact sequence order, which preserves deterministic downstream state."
])
flow += [p("Decision sequence", "H2x"), table([
    ["Condition", "Action"],
    ["sequence < expected", "Count duplicate; discard"],
    ["sequence == expected", "Deliver and drain consecutive buffered messages"],
    ["sequence > expected", "Buffer; request [expected, sequence-1]"],
    ["window collision / capacity", "Fail explicitly; never overwrite silently"],
], [55*mm, 116*mm]), PageBreak()]
section(flow, "4. Parse ITCH into HotEvent", [
    "HotItchParser validates message type and exact length. Order-impacting messages become a fixed-width HotEvent containing numeric fields plus 8-byte symbol and 4-byte attribution arrays.",
    "Administrative and auction messages are validated but represented as informational on the order-building hot path. The full typed parser remains available for control-plane consumers."
])
flow += [p("Order state transitions", "H2x"), table([
    ["ITCH", "Book effect"],
    ["A / F", "Create order and increase price level"],
    ["E / C", "Reduce shares; delete fully executed order"],
    ["X", "Cancel shares"],
    ["D", "Delete remaining order"],
    ["U", "Atomically replace old reference with new reference/price/size"],
], [35*mm, 136*mm])]
section(flow, "5. SPSC fan-out", [
    "The parser is the single producer for book, recorder, and metrics queues. Each queue has one dedicated consumer. Acquire/release publication makes the event visible without mutexes.",
    "High-watermarks show the maximum observed occupancy. They are evidence for capacity planning and reveal which consumer is limiting throughput."
], "Queues are bounded. Bounded systems expose overload; they do not hide it behind unbounded memory growth.")
section(flow, "6. Book, recorder, metrics, and PCAP", [
    "The arena-book worker is the only owner of mutable book state. Hash nodes come from a recycling PMR pool backed by a fixed arena with no general-heap fallback.",
    "The recorder worker materializes legacy string-bearing events only after leaving the hot path. The PCAP worker writes raw packet copies asynchronously. The metrics worker updates progress independently."
])
flow += [PageBreak()]
section(flow, "7. Checkpoint, restart, and replay", [
    "CheckpointStore writes two alternating generations with checksums, file flush, atomic replacement, and directory durability. On restart it selects the newest valid generation and falls back if corruption is detected.",
    "PCAP and normalized tick replay preserve recorded order. The deterministic simulator and recovery source reproduce packet loss, retransmission, restart, and final book equivalence."
])
section(flow, "8. Shutdown sequence", [
    "Shutdown stops new ingress, drains the parser, marks parser completion, drains every consumer, flushes and closes recorders, joins threads, and publishes final book counts.",
    "This ordering prevents a successful shutdown from silently abandoning accepted work."
])
flow += [p("Operational success criteria", "H1x")] + bullets([
    "packets_submitted equals the intended accepted input count",
    "messages_parsed, book_updates, and metrics_events agree for one-message simulator packets",
    "rejected packets, parse errors, and unrecovered gaps are zero for a clean run",
    "checkpoint restart produces identical symbol and active-order counts",
    "queue watermarks remain below sustained saturation with production headroom",
]) + [p("Evidence boundary", "H1x"), p(
    "The repository proves software behavior in CI. Licensed-feed compatibility, NIC line rate, PTP timestamp accuracy, and FPGA DMA performance require the actual provider network and hardware.", "Callout")]
build_pdf("MarketCapture_System_Flow.pdf", "MarketCapture System Flow", flow)

design = cover("Engineering Design and Rationale", "Component-by-component design choices, alternatives, invariants, and failure behavior",
               "Detailed reference for understanding, modifying, and defending the system")
design += [p("Design principles", "H1x")] + bullets([
    "Determinism before throughput: never update a book across an unresolved sequence gap.",
    "Bounded memory: every queue, packet pool, reorder window, and arena has an explicit capacity.",
    "Single ownership: one writer owns each mutable structure.",
    "Evidence over claims: measurements identify host, compiler, configuration, and workload.",
    "Hardware adapters are separated from portable protocol and state logic.",
]) + [PageBreak()]
chapters = [
("1. Why C++20", "C++ provides deterministic object lifetime, direct memory layout control, atomics, spans, PMR allocators, and access to kernel and DPDK APIs. C++20 adds std::span, atomic improvements, jthread-era concurrency facilities, and clearer compile-time constraints. The tradeoff is that lifetime and synchronization correctness are the developer's responsibility."),
("2. Why MoldUDP64", "MoldUDP64 batches length-prefixed messages under a session and sequence header. Sequence identity enables duplicate suppression and gap detection, while batching amortizes network overhead. The decoder is bounds checked because a single malformed length could otherwise turn packet loss into memory corruption."),
("3. Why two parsers", "ItchParser returns rich typed events with strings and is ideal for completeness, tools, and correctness comparisons. HotItchParser returns a fixed HotEvent and avoids string allocation on the order path. The independent golden decoder checks that optimization did not silently change protocol meaning."),
("4. Why fixed-width symbols", "NASDAQ stock fields are fixed at eight bytes and MPID attribution is four bytes. Preserving those widths avoids heap allocation and makes queue elements predictable. Trimming into std::string occurs only when a cold consumer needs presentation or legacy serialization."),
("5. Why circular arbitration", "A std::map naturally orders sequences but allocates nodes and adds pointer chasing. A modulo-indexed window has fixed memory and constant-time lookup. It is safe only while the configured reorder distance is respected; a collision is treated as capacity failure rather than overwrite."),
("6. Why SPSC queues", "Each queue has one producer and one consumer, allowing head and tail cursors without locks or compare-and-swap loops. Producer release publishes a completed slot; consumer acquire observes it. Cache-line alignment reduces false sharing. SPSC is deliberately not advertised as MPMC."),
("7. Why value fan-out", "Each downstream worker receives an owned HotEvent. This prevents a slow recorder from retaining the NIC packet and keeps lifetime reasoning simple. The cost is a small fixed copy, chosen over reference counting and cross-thread reclamation complexity."),
("8. Why an arena plus recycling pool", "General-purpose allocation adds latency variance and may lock internally. A fixed arena bounds memory and a PMR pool reuses erased hash nodes during order churn. null_memory_resource prevents hidden heap fallback. Capacity planning is therefore an operational requirement."),
("9. Why a global order index", "Execute, cancel, delete, and replace messages identify an order reference rather than a symbol. A global order map gives constant-time routing to symbol, side, price, and remaining shares. Price-level keys aggregate total shares and order count."),
("10. Why asynchronous recording", "Disk write, compression, page faults, and fsync have long-tail latency. Recorder and PCAP workers isolate those effects from RX and parsing. Separate queues also expose storage lag through watermarks instead of contaminating packet latency invisibly."),
("11. Why two checkpoint generations", "An in-place checkpoint can be torn by power loss. Alternating slots retain a previous valid generation. Checksums detect corruption; fsync and atomic rename establish durability ordering. The design favors simple auditable recovery over a complex transactional database."),
("12. Why PCAP plus normalized ticks", "PCAP preserves exact ingress bytes for protocol debugging and recovery. Normalized ticks are smaller and convenient for deterministic book replay. Keeping both separates wire evidence from application representation."),
("13. Why recvmmsg, io_uring, and DPDK", "recvmmsg is a simple batched kernel path. Multishot io_uring reduces submission overhead and uses provided buffers. DPDK bypasses the kernel and is appropriate when NIC ownership, huge pages, and dedicated cores are available. They are alternatives selected by deployment evidence."),
("14. Why thread affinity", "Pinning prevents migration-related cache disruption and makes measurements reproducible. It is optional because incorrect pinning can compete with NIC IRQs or place memory on the wrong NUMA node. CPU IDs are configuration, not hard-coded policy."),
("15. Why explicit overload policies", "Reject protects the RX thread and exposes loss immediately. Spin preserves work only if upstream behavior and CPU budget allow backpressure. Unbounded queues are rejected because they convert overload into eventual memory failure and enormous tail latency."),
("16. Why percentiles and watermarks", "Mean latency hides rare stalls. p50 describes the center; p99 and p99.9 reveal tail behavior. Queue watermarks identify accumulating work. NIC missed packets, application rejects, gaps, parse errors, and storage lag must be interpreted together."),
("17. Why fuzzers and sanitizers", "Binary decoders face adversarial lengths and truncation. Prefix tests, libFuzzer, ASan, and UBSan cover different failure classes. They do not prove correctness, so golden and official-file tests provide independent semantic evidence."),
("18. Why deterministic simulation", "A seeded simulator generates reproducible order lifecycles and A/B loss patterns. It makes bugs repeatable and CI friendly. It is not a substitute for independently produced NASDAQ data, which is why the BinaryFile validator exists."),
("19. Hardware boundaries", "SO_TIMESTAMPING, DPDK, and the FPGA DMA ABI are real adapters, but compile success is not physical performance proof. Line rate, timestamp accuracy, NUMA behavior, and DMA coherency must be measured on the selected NIC, clock, driver, and FPGA."),
("20. Security and operations", "Feed configuration contains network assignments, not portal credentials. Capture files may contain commercially controlled data. Production service management must cover permissions, disk alarms, retention export, log rotation, restart policy, and secret handling outside source control."),
]
for idx, (title, body) in enumerate(chapters):
    design.append(p(title, "H1x"))
    design.append(p(body))
    if idx in (4, 8, 12, 16): design.append(PageBreak())
design += [PageBreak(), p("Component contract matrix", "H1x"), table([
    ["Component", "Invariant", "Failure signal"],
    ["Packet pool", "One submitter; slot returned after parse", "submit=false / reject counter"],
    ["Arbitrator", "Only expected sequence is delivered", "recovery request / window error"],
    ["Hot parser", "Exact type and length", "ParseError"],
    ["SPSC", "Exactly one producer and consumer", "full queue / watermark"],
    ["Arena book", "Order and level totals agree", "capacity or transition exception"],
    ["Recorder", "Only order-impacting events", "worker exception"],
    ["Checkpoint", "Newest checksum-valid generation", "fallback or no-valid-generation"],
], [33*mm, 93*mm, 45*mm])]
design += [p("Performance interpretation", "H1x"), p(
    "A benchmark number is valid only with its CPU model, kernel, compiler, optimization flags, affinity, NUMA policy, governor, workload size, queue policy, active-order population, and recorder configuration. A faster synthetic parser number does not prove end-to-end market-feed capacity.", "Callout")]
design += [p("Modification checklist", "H1x")] + bullets([
    "State the invariant the change preserves.",
    "Add malformed, capacity, shutdown, and replay tests.",
    "Run sanitizers and fuzz smoke.",
    "Measure same-host before and after distributions.",
    "Inspect flamegraph and queue watermarks.",
    "Document any new deployment assumption or external dependency.",
])
build_pdf("MarketCapture_Engineering_Design.pdf", "MarketCapture Engineering Design", design)

interview = cover("Interview Preparation Handbook", "Architecture walkthrough, questions, model answers, and live-debugging plan",
                  "For C++ systems, market data, low-latency, and HFT interviews")
interview += [p("Your 60-second introduction", "H1x"), p(
    "I built MarketCapture, a C++20 NASDAQ ITCH 5.0 market-data platform. It receives MoldUDP64 through socket, multishot io_uring, or DPDK paths; arbitrates redundant A/B feeds; reconstructs bounded-memory order books; records PCAP and normalized ticks asynchronously; checkpoints state; and replays deterministically. The hot path uses fixed-width events, preallocated packet storage, circular reordering, SPSC queues, and a fixed PMR arena. Correctness is protected by independent golden decoding, fuzzing, sanitizers, recovery tests, and Linux CI.", "Callout")]
qa = [
("What is the most important invariant?", "The book never receives a message after an unresolved sequence gap. Deterministic sequence order is more important than processing immediately."),
("Why not use a mutex-protected queue?", "The topology is one producer and one consumer per edge. SPSC needs only acquire/release cursor publication, avoiding lock contention and unpredictable wakeups."),
("Explain acquire/release here.", "The producer writes the slot, then stores head with release. The consumer loads head with acquire, so it observes the completed slot. Tail uses the symmetric pattern for reuse."),
("What happens when a queue fills?", "The configured policy either rejects ingress or spins. Downstream parser fan-out yields. Counters and high-watermarks expose overload; no queue grows without bound."),
("Why is A/B arbitration necessary?", "Both feeds can duplicate, delay, or lose packets independently. Arbitration emits one ordered stream, suppresses duplicates, and requests missing ranges."),
("Why copy out-of-order messages?", "The NIC packet slot must be returned. A bounded fixed copy decouples lifetime without heap allocation or reference-counted packet retention."),
("Why fixed HotEvent instead of std::variant strings?", "Fixed size makes queue storage predictable and avoids allocator variance. The rich typed parser remains for control-plane completeness and differential validation."),
("How does an add change the book?", "Create the order-index entry and increment shares/order count at symbol-side-price. Duplicate reference, zero size, or exhausted capacity is rejected."),
("How is replace handled?", "Validate old exists and new does not, remove the old remaining quantity, then insert the new reference using the old side/symbol and new price/size."),
("Why a PMR pool over monotonic allocation?", "Pure monotonic memory never reuses cancelled nodes. The pool recycles same-size nodes while its fixed monotonic upstream bounds total memory."),
("How do you recover after a crash?", "Load the newest checksum-valid checkpoint generation, fall back to the previous slot if needed, then replay ordered records after the saved sequence."),
("Why both PCAP and tick logs?", "PCAP is wire truth for decoder/debugging; normalized ticks are compact application events for fast deterministic replay."),
("What does deterministic replay mean?", "Given the same validated ordered events and checkpoint boundary, state transitions and resulting book counts are identical. Wall-clock scheduling is not claimed deterministic."),
("Why p99.9?", "Rare stalls matter in low latency. A mean can look healthy while one in a thousand messages suffers a severe delay."),
("recvmmsg vs io_uring?", "recvmmsg is simpler batching. Multishot io_uring amortizes submissions with provided buffers. Benchmark on the target kernel because complexity is not automatically faster."),
("When use DPDK?", "When dedicated NIC queues, huge pages, CPU isolation, and operational ownership justify kernel bypass. Otherwise the kernel path may be simpler and sufficient."),
("What does DPDK compile CI prove?", "API integration and build compatibility only. It does not prove physical line rate, NUMA placement, or NIC behavior."),
("How do you validate official data?", "Verify NASDAQ MD5, stream-decompress BinaryFile records, run full and hot parsers, compare classification, count types, and publish the exact dataset/commit report."),
("How do fuzzing and golden tests differ?", "Fuzzing explores malformed boundaries and crashes. Golden tests compare known semantics against an independent implementation."),
("Where can allocation still occur?", "Construction and cold materialization allocate. Steady-state arbitration/events/book nodes use preallocated bounded storage; capacity failure is explicit."),
("What would you optimize next?", "Use bare-metal flamegraphs and watermarks to select the bottleneck. Likely targets depend on active-order population, hashing, recorder pressure, or RX batching."),
("How would you debug a missing order?", "Trace sequence, channel, arbitration slot, parsed type/order ID, router lookup, and prior lifecycle. Replay the PCAP with deterministic logging."),
("How do you prevent false benchmark claims?", "Pin conditions, preserve raw outputs, compare same-host runs, state workload and active state size, and separate software CI from physical hardware evidence."),
("What is not complete without external access?", "Licensed multicast validation, physical DPDK/FPGA results, PTP timestamp accuracy, and a published official multi-GB dataset run."),
]
for i, (q, a) in enumerate(qa, 1):
    interview.append(KeepTogether([p(f"{i}. {q}", "H2x"), p(a)]))
    if i in (8, 16): interview.append(PageBreak())
interview += [PageBreak(), p("Live coding and debugging route", "H1x"), p(
    "Build Debug, run marketcapture_threaded with a small packet count, and place breakpoints in ThreadedCaptureEngine::submit, FeedArbitrator::ingest/drain, HotItchParser::parse, ArenaBookRouter::apply, and the recorder loop. Watch expected sequence, queue size, order ID, symbol bytes, side, price, shares, and watermarks.", "Bodyx"),
    p("cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug<br/>cmake --build build-debug<br/>ctest --test-dir build-debug --output-on-failure", "CodeX"),
    p("Whiteboard prompt", "H1x"), p(
        "Draw: A/B NIC input -> packet pool -> arbitration -> fixed parser -> three event SPSC queues plus raw PCAP queue -> arena book / recorder / metrics / PCAP workers. Mark ownership, capacity, sequence invariant, and shutdown direction.", "Callout"),
    p("Questions to ask the interviewer", "H1x")] + bullets([
        "What peak and burst message rates define capacity?",
        "How are feed gaps recovered and when is a book considered trustworthy?",
        "Which latency boundary is measured: NIC, parser, book, or strategy callback?",
        "How are CPU isolation, NUMA, IRQ affinity, and PTP managed?",
        "What correctness and replay evidence is required before production release?",
    ])
build_pdf("MarketCapture_Interview_Handbook.pdf", "MarketCapture Interview Handbook", interview)

print("\n".join(str(path) for path in sorted(OUT.glob("MarketCapture_*.pdf"))))
