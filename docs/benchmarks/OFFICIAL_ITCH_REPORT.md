# Official NASDAQ ITCH correctness report

Status: workflow implemented; public multi-gigabyte dataset result pending an
explicit dataset URL/checksum and sufficient local storage.

Official sources:

- <https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/>
- <https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification_5.0.pdf>

The validator streams NASDAQ BinaryFile framing, runs both the full parser and
the fixed hot parser, checks order/non-order classification, and emits message
counts by type. It never loads the session into memory.

```sh
bash scripts/fetch_official_itch.sh \
  'https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/<file>.gz' \
  '<md5-from-official-directory>' data/<file>.gz
gzip -dc data/<file>.gz | ./build/marketcapture_validate_itch - \
  | tee evidence/official-itch.txt
```

Acceptance: checksum matches NASDAQ, zero framing/parser errors, every record
has a recognized ITCH 5.0 type, and full/hot parser classification agrees.
Record filename, MD5, UTC run time, commit, counts, and runtime here after the
real run. The dataset is excluded from source control.
