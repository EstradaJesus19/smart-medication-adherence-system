"""Read-only validation of scheduled_sd_test CSV segments. Never repairs a file."""
import argparse
import io
from pathlib import Path
import re
import zlib

HEADER = b"v,boot_id,seq,uptime_ms,rtc_s2000,rtc_quality,origin,event,compartment,scheduled_at_s2000,session_at_s2000,due_mask,state,dropped,uncertain,detail,crc32\n"
MAX_LINE = 384

def inspect(stream):
    result = dict(ok=False, rows=0, gaps=0, first_seq=None, last_seq=None, boot=None,
                  dropped=0, uncertain=0, error="")
    if stream.readline(MAX_LINE + 1) != HEADER:
        result["error"] = "missing/partial/unsupported header"
        return result
    number = 1
    while True:
        raw = stream.readline(MAX_LINE + 1)
        if not raw:
            result["ok"] = True
            return result
        number += 1
        try:
            if len(raw) >= MAX_LINE or not raw.endswith(b"\n"):
                raise ValueError("oversized or incomplete record/tail")
            payload, checksum = raw[:-1].rsplit(b",", 1)
            if not re.fullmatch(rb"[0-9a-f]{8}", checksum) or int(checksum,16) != zlib.crc32(payload):
                raise ValueError("checksum mismatch")
            fields = payload.decode("ascii").split(",")
            if len(fields) != 16 or fields[0] != "1":
                raise ValueError("unsupported schema/field count")
            if not re.fullmatch(r"[0-9a-f]{16}", fields[1]):
                raise ValueError("invalid boot ID")
            ints = {}
            for i in (2,3,4,8,9,10,11,13,14):
                if not re.fullmatch(r"[0-9]+", fields[i]):
                    raise ValueError("invalid numeric field")
                ints[i] = int(fields[i])
                limit = (1<<64)-1 if i in (2,13,14) else (1<<32)-1
                if ints[i] > limit:
                    raise ValueError("numeric range")
            if ints[2] == 0 or ints[8] > 4 or ints[11] > 15:
                raise ValueError("sequence/compartment/mask range")
            if fields[5] not in ("valid","untrusted","stale") or (fields[5] != "valid" and ints[4] != 0):
                raise ValueError("invalid RTC quality/value")
            if fields[6] not in ("core","scheduler","control","test","logger"):
                raise ValueError("invalid origin")
            if not re.fullmatch(r"[A-Z0-9_]{1,39}", fields[7]) or not re.fullmatch(r"[A-Za-z0-9_:.-]{1,39}", fields[15]):
                raise ValueError("invalid event/detail")
            if fields[12] not in ("IDLE","REMINDER","WAITING_FOR_CLOSURE","REFILL"):
                raise ValueError("invalid state")
            if result["boot"] is not None and fields[1] != result["boot"]:
                raise ValueError("mixed boot IDs")
            if result["last_seq"] is not None and ints[2] <= result["last_seq"]:
                raise ValueError("duplicate/backward sequence")
            if ints[13] < result["dropped"] or ints[14] < result["uncertain"]:
                raise ValueError("backward loss counters")
        except (ValueError, UnicodeError) as error:
            result["error"] = f"line {number}: {error}; earlier {result['rows']} rows form the valid prefix (file unchanged)"
            return result
        result["boot"] = fields[1]
        if result["first_seq"] is None:
            result["first_seq"] = ints[2]
        if result["last_seq"] is not None:
            result["gaps"] += ints[2] - result["last_seq"] - 1
        result["last_seq"] = ints[2]
        result["dropped"], result["uncertain"] = ints[13], ints[14]
        result["rows"] += 1

def packet(fields):
    payload = ",".join(fields).encode("ascii")
    return payload + f",{zlib.crc32(payload):08x}\n".encode("ascii")

def self_test():
    fields = ["1","123456789abcdef0","1","0","0","untrusted","logger","BOOT","0","0","0","0","IDLE","0","0","-"]
    row = packet(fields)
    assert inspect(io.BytesIO(HEADER+row))["ok"]
    assert not inspect(io.BytesIO(HEADER+row[:-1]))["ok"]
    changed = row.replace(b"BOOT",b"BOOX")
    assert not inspect(io.BytesIO(HEADER+changed))["ok"]
    duplicate = inspect(io.BytesIO(HEADER+row+row))
    assert not duplicate["ok"] and duplicate["rows"] == 1
    later = fields.copy(); later[2] = "3"; later[13] = "1"
    gap = inspect(io.BytesIO(HEADER+row+packet(later)))
    assert gap["ok"] and gap["gaps"] == 1 and gap["dropped"] == 1
    later[1] = "aaaaaaaaaaaaaaaa"
    assert not inspect(io.BytesIO(HEADER+row+packet(later)))["ok"]
    later = fields.copy(); later[4] = "123"
    assert not inspect(io.BytesIO(HEADER+packet(later)))["ok"]
    assert not inspect(io.BytesIO(HEADER[:-1]))["ok"]
    assert inspect(io.BytesIO(HEADER))["ok"]
    print("CSV validator checks passed: CRC, partial tail, valid prefix, duplicate, gaps, boot identity, RTC trust")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("files", nargs="*", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
    if not args.files and not args.self_test:
        parser.error("select one or more CSV segment files")
    failed = False
    for path in args.files:
        try:
            with path.open("rb") as stream:
                result = inspect(stream)
        except OSError as error:
            print(f"FAIL {path}: {error}"); failed = True; continue
        label = "PASS" if result["ok"] else "FAIL"
        print(f"{label} {path}: {result}")
        if result["first_seq"] and result["first_seq"] > 1:
            print("  Segment starts after sequence 1; inspect earlier segments/status for previous or missing events.")
        failed |= not result["ok"]
    return int(failed)

if __name__ == "__main__":
    raise SystemExit(main())
