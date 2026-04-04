#!/bin/bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  verify-aliyun-turn-relay.sh <peer-host> [ssh-key]

Examples:
  verify-aliyun-turn-relay.sh <OFFICE_SERVER_1>
  verify-aliyun-turn-relay.sh <OFFICE_SERVER_2>
  verify-aliyun-turn-relay.sh <OFFICE_SERVER_2> ~/.ssh/id_rsa_<OFFICE_SERVER_2>

Environment overrides:
  SSH_USER     Remote SSH user, default: lhb
  RELAY_HOST   TURN/STUN server host, default: <YOUR_SERVER_IP>
  TURN_PORT    TURN UDP port, default: 9001
  STUN_PORT    STUN UDP port, default: 3478
  PEER_LABEL   Label shown in payloads, default: derived from peer-host
  SSH_TIMEOUT_SEC  Hard timeout for each local ssh call, default: 20
  KEEP_REMOTE  Keep remote temp files for debugging, default: 0
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -lt 1 || $# -gt 2 ]]; then
    usage >&2
    exit 1
fi

PEER_HOST="$1"
case "$PEER_HOST" in
    <OFFICE_SERVER_1>)
        DEFAULT_KEY="$HOME/.ssh/id_rsa_<OFFICE_SERVER_1>"
        ;;
    <OFFICE_SERVER_2>)
        DEFAULT_KEY="$HOME/.ssh/id_rsa_<OFFICE_SERVER_2>"
        ;;
    *)
        DEFAULT_KEY="$HOME/.ssh/id_rsa"
        ;;
esac

SSH_KEY="${2:-$DEFAULT_KEY}"
SSH_USER="${SSH_USER:-lhb}"
RELAY_HOST="${RELAY_HOST:-<YOUR_SERVER_IP>}"
TURN_PORT="${TURN_PORT:-9001}"
STUN_PORT="${STUN_PORT:-3478}"
PEER_LABEL="${PEER_LABEL:-${PEER_HOST##*.}}"
SSH_TIMEOUT_SEC="${SSH_TIMEOUT_SEC:-20}"
KEEP_REMOTE="${KEEP_REMOTE:-0}"
REMOTE_TARGET="${SSH_USER}@${PEER_HOST}"

if ! command -v ssh >/dev/null 2>&1; then
    echo "ERROR: ssh is required" >&2
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: python3 is required" >&2
    exit 1
fi

if [[ ! -f "$SSH_KEY" ]]; then
    echo "ERROR: SSH key not found: $SSH_KEY" >&2
    exit 1
fi

ssh_opts=(
    -i "$SSH_KEY"
    -o BatchMode=yes
    -o StrictHostKeyChecking=no
    -o ConnectTimeout=5
    -o ServerAliveInterval=5
    -o ServerAliveCountMax=3
)

run_ssh_cmd() {
    python3 -c '
import select
import subprocess
import sys

timeout = float(sys.argv[1])
argv = sys.argv[2:]
kwargs = {"text": True, "capture_output": True, "timeout": timeout}
if not sys.stdin.closed:
    ready, _, _ = select.select([sys.stdin], [], [], 0)
    if ready:
        stdin_text = sys.stdin.read()
        if stdin_text:
            kwargs["input"] = stdin_text
try:
    completed = subprocess.run(argv, **kwargs)
except subprocess.TimeoutExpired as exc:
    if exc.stdout:
        sys.stdout.write(exc.stdout)
    if exc.stderr:
        sys.stderr.write(exc.stderr)
    print(f"ERROR: ssh command timed out after {timeout:g}s", file=sys.stderr)
    raise SystemExit(124)
sys.stdout.write(completed.stdout or "")
sys.stderr.write(completed.stderr or "")
raise SystemExit(completed.returncode)
' "$SSH_TIMEOUT_SEC" "$@"
}

echo "==> Preparing remote peer on $REMOTE_TARGET"
REMOTE_DIR="$(run_ssh_cmd ssh "${ssh_opts[@]}" "$REMOTE_TARGET" "mktemp -d /tmp/alibaba-relay-peer.XXXXXX")"
if [[ -z "$REMOTE_DIR" ]]; then
    echo "ERROR: failed to create remote temp directory" >&2
    exit 1
fi
REMOTE_STATE="$REMOTE_DIR/state.json"
REMOTE_CMD="$REMOTE_DIR/relay_endpoint.json"
REMOTE_LOG="$REMOTE_DIR/peer.log"

remote_cleanup() {
    if [[ "$KEEP_REMOTE" == "1" ]]; then
        return
    fi
    run_ssh_cmd ssh "${ssh_opts[@]}" "$REMOTE_TARGET" "rm -rf '$REMOTE_DIR'" >/dev/null 2>&1 || true
}

trap remote_cleanup EXIT

cat <<'PY' | run_ssh_cmd ssh "${ssh_opts[@]}" "$REMOTE_TARGET" "cat > '$REMOTE_DIR/peer.py'"
import json
import os
import socket
import struct
import sys
import tempfile
import time

MAGIC_COOKIE = 0x2112A442
BASE_DIR = sys.argv[1]
LABEL = sys.argv[2]
HOST = sys.argv[3]
STUN_PORT = int(sys.argv[4])
STATE = os.path.join(BASE_DIR, "state.json")
CMD = os.path.join(BASE_DIR, "relay_endpoint.json")


def save(state):
    state["updated_at"] = time.time()
    fd, temp_path = tempfile.mkstemp(prefix="state.", dir=BASE_DIR)
    with os.fdopen(fd, "w", encoding="utf-8") as handle:
        json.dump(state, handle)
    os.replace(temp_path, STATE)


def parse_stun_xor_mapped(data):
    if len(data) < 20:
        return None
    msg_len = struct.unpack("!H", data[2:4])[0]
    offset = 20
    end = 20 + msg_len
    while offset + 4 <= end and offset + 4 <= len(data):
        attr_type, attr_len = struct.unpack("!HH", data[offset:offset + 4])
        offset += 4
        value = data[offset:offset + attr_len]
        offset += (attr_len + 3) & ~3
        if attr_type == 0x0020 and len(value) >= 8:
            xor_port = struct.unpack("!H", value[2:4])[0]
            port = xor_port ^ ((MAGIC_COOKIE >> 16) & 0xFFFF)
            cookie_bytes = struct.pack("!I", MAGIC_COOKIE)
            ip_bytes = bytes(a ^ b for a, b in zip(value[4:8], cookie_bytes))
            return {"host": socket.inet_ntoa(ip_bytes), "port": port}
    return None


for path in [STATE, CMD]:
    try:
        os.remove(path)
    except FileNotFoundError:
        pass

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", 0))
sock.settimeout(20)
state = {"status": "bound", "label": LABEL, "local_port": sock.getsockname()[1]}
save(state)

try:
    transaction_id = os.urandom(12)
    binding_req = struct.pack("!HHI", 0x0001, 0, MAGIC_COOKIE) + transaction_id
    sock.sendto(binding_req, (HOST, STUN_PORT))
    data, addr = sock.recvfrom(2048)
    public_ep = parse_stun_xor_mapped(data)
    state.update(
        {
            "status": "stun_ok",
            "stun_server": [addr[0], addr[1]],
            "public_endpoint": public_ep,
        }
    )
    save(state)

    deadline = time.time() + 90
    while time.time() < deadline and not os.path.exists(CMD):
        time.sleep(0.2)
    if not os.path.exists(CMD):
        state["status"] = "timeout_waiting_relay_endpoint"
        save(state)
        raise SystemExit(1)

    with open(CMD, "r", encoding="utf-8") as handle:
        relay = json.load(handle)
    hello = ("peer-hello-from-" + LABEL).encode("utf-8")
    sock.sendto(hello, (relay["host"], relay["port"]))
    state.update(
        {
            "status": "hello_sent",
            "relay_endpoint": relay,
            "hello_payload": hello.decode("utf-8"),
        }
    )
    save(state)

    data, addr = sock.recvfrom(4096)
    state.update(
        {
            "status": "received",
            "from": [addr[0], addr[1]],
            "received_text": data.decode("utf-8", "replace"),
        }
    )
    save(state)

    reply = ("peer-reply-from-" + LABEL).encode("utf-8")
    sock.sendto(reply, addr)
    state.update({"status": "replied", "reply_text": reply.decode("utf-8")})
    save(state)
except Exception as exc:  # pragma: no cover - helper script path
    state.update({"status": "error", "error": repr(exc)})
    save(state)
    raise
finally:
    sock.close()
PY

echo "==> Starting remote peer and waiting for STUN mapping"
printf -v remote_dir_q '%q' "$REMOTE_DIR"
printf -v peer_label_q '%q' "$PEER_LABEL"
printf -v relay_host_q '%q' "$RELAY_HOST"
printf -v stun_port_q '%q' "$STUN_PORT"
printf -v remote_state_q '%q' "$REMOTE_STATE"
printf -v remote_cmd_q '%q' "$REMOTE_CMD"
printf -v remote_log_q '%q' "$REMOTE_LOG"
printf -v remote_peer_q '%q' "$REMOTE_DIR/peer.py"
run_ssh_cmd ssh "${ssh_opts[@]}" "$REMOTE_TARGET" \
    "rm -f $remote_state_q $remote_cmd_q $remote_log_q && \
    setsid -f python3 $remote_peer_q $remote_dir_q $peer_label_q $relay_host_q $stun_port_q >$remote_log_q 2>&1"

stun_ok=0
for _ in $(seq 1 30); do
    if remote_state="$(run_ssh_cmd ssh "${ssh_opts[@]}" "$REMOTE_TARGET" \
        "python3 - <<'PY'
import json
import os
import time
path = '$REMOTE_STATE'
for _ in range(5):
    try:
        with open(path, 'r', encoding='utf-8') as handle:
            print(json.dumps(json.load(handle)))
            raise SystemExit(0)
    except FileNotFoundError:
        time.sleep(0.1)
    except json.JSONDecodeError:
        time.sleep(0.1)
raise SystemExit(1)
PY")"; then
        echo "$remote_state"
        if python3 -c 'import json, sys; sys.exit(0 if json.loads(sys.stdin.read()).get("status") == "stun_ok" else 1)' \
            <<<"$remote_state" >/dev/null; then
            stun_ok=1
            break
        fi
    fi
    sleep 0.5
done

if [[ "$stun_ok" -ne 1 ]]; then
    echo "ERROR: remote peer did not reach stun_ok state" >&2
    run_ssh_cmd ssh "${ssh_opts[@]}" "$REMOTE_TARGET" "test -f '$REMOTE_LOG' && sed -n '1,120p' '$REMOTE_LOG'" || true
    exit 1
fi

echo "==> Running TURN relay validation"
PEER_HOST="$PEER_HOST" \
PEER_LABEL="$PEER_LABEL" \
RELAY_HOST="$RELAY_HOST" \
TURN_PORT="$TURN_PORT" \
REMOTE_TARGET="$REMOTE_TARGET" \
SSH_KEY="$SSH_KEY" \
SSH_TIMEOUT_SEC="$SSH_TIMEOUT_SEC" \
REMOTE_STATE="$REMOTE_STATE" \
REMOTE_CMD="$REMOTE_CMD" \
python3 - <<'PY'
import json
import os
import socket
import shlex
import struct
import subprocess
import sys
import time

MAGIC_COOKIE = 0x2112A442


def run_ssh(remote_target, ssh_key, remote_command):
    completed = subprocess.run(
        [
            "ssh",
            "-i",
            ssh_key,
            "-o",
            "BatchMode=yes",
            "-o",
            "StrictHostKeyChecking=no",
            "-o",
            "ConnectTimeout=5",
            "-o",
            "ServerAliveInterval=5",
            "-o",
            "ServerAliveCountMax=3",
            remote_target,
            remote_command,
        ],
        check=True,
        text=True,
        capture_output=True,
        timeout=float(os.environ["SSH_TIMEOUT_SEC"]),
    )
    return completed.stdout.strip()


def transaction_id():
    return os.urandom(12)


def pad4(value):
    return value + (b"\x00" * ((4 - len(value) % 4) % 4))


def attr(attr_type, value):
    return struct.pack("!HH", attr_type, len(value)) + pad4(value)


def xor_addr(ip, port):
    ip_bytes = socket.inet_aton(ip)
    xor_port = port ^ ((MAGIC_COOKIE >> 16) & 0xFFFF)
    cookie_bytes = struct.pack("!I", MAGIC_COOKIE)
    xor_ip = bytes(a ^ b for a, b in zip(ip_bytes, cookie_bytes))
    return b"\x00\x01" + struct.pack("!H", xor_port) + xor_ip


def stun(msg_type, tid, attrs):
    body = b"".join(attrs)
    return struct.pack("!HHI", msg_type, len(body), MAGIC_COOKIE) + tid + body


def parse(data):
    msg_type, msg_len, cookie = struct.unpack("!HHI", data[:8])
    if cookie != MAGIC_COOKIE:
        raise RuntimeError("invalid STUN/TURN cookie")
    attrs = []
    offset = 20
    end = 20 + msg_len
    while offset + 4 <= end and offset + 4 <= len(data):
        attr_type, attr_len = struct.unpack("!HH", data[offset:offset + 4])
        offset += 4
        value = data[offset:offset + attr_len]
        attrs.append((attr_type, value))
        offset += (attr_len + 3) & ~3
    return msg_type, attrs


def parse_xor_addr(value):
    xor_port = struct.unpack("!H", value[2:4])[0]
    port = xor_port ^ ((MAGIC_COOKIE >> 16) & 0xFFFF)
    cookie_bytes = struct.pack("!I", MAGIC_COOKIE)
    ip_bytes = bytes(a ^ b for a, b in zip(value[4:8], cookie_bytes))
    return socket.inet_ntoa(ip_bytes), port


def read_remote_json(remote_target, ssh_key, remote_path):
    remote_command = (
        f"REMOTE_PATH={shlex.quote(remote_path)} python3 - <<'PY2'\n"
        f"import json\n"
        f"import os\n"
        f"import time\n"
        f"path = os.environ['REMOTE_PATH']\n"
        f"for _ in range(10):\n"
        f"    try:\n"
        f"        with open(path, 'r', encoding='utf-8') as handle:\n"
        f"            print(json.dumps(json.load(handle)))\n"
        f"            raise SystemExit(0)\n"
        f"    except FileNotFoundError:\n"
        f"        time.sleep(0.1)\n"
        f"    except json.JSONDecodeError:\n"
        f"        time.sleep(0.1)\n"
        f"raise SystemExit(1)\n"
        f"PY2"
    )
    return json.loads(run_ssh(remote_target, ssh_key, remote_command))


def write_remote_json(remote_target, ssh_key, remote_path, payload):
    encoded = json.dumps(payload, ensure_ascii=False)
    remote_command = (
        f"REMOTE_PATH={shlex.quote(remote_path)} PAYLOAD={shlex.quote(encoded)} python3 - <<'PY2'\n"
        f"import json\n"
        f"import os\n"
        f"path = os.environ['REMOTE_PATH']\n"
        f"payload = json.loads(os.environ['PAYLOAD'])\n"
        f"temp_path = path + '.tmp'\n"
        f"with open(temp_path, 'w', encoding='utf-8') as handle:\n"
        f"    json.dump(payload, handle)\n"
        f"os.replace(temp_path, path)\n"
        f"PY2"
    )
    run_ssh(remote_target, ssh_key, remote_command)


relay_host = os.environ["RELAY_HOST"]
turn_port = int(os.environ["TURN_PORT"])
remote_target = os.environ["REMOTE_TARGET"]
ssh_key = os.environ["SSH_KEY"]
remote_state_path = os.environ["REMOTE_STATE"]
remote_cmd_path = os.environ["REMOTE_CMD"]
peer_label = os.environ["PEER_LABEL"]

result = {}

client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
client.bind(("0.0.0.0", 0))
client.settimeout(6)

try:
    result["peer_state_initial"] = read_remote_json(remote_target, ssh_key, remote_state_path)

    alloc_tid = transaction_id()
    client.sendto(stun(0x0003, alloc_tid, [attr(0x000D, struct.pack("!I", 600))]), (relay_host, turn_port))
    alloc_resp, _ = client.recvfrom(2048)
    msg_type, attrs = parse(alloc_resp)
    if msg_type != 0x0103:
        raise RuntimeError(f"unexpected allocate response: 0x{msg_type:04x}")

    relay_endpoint = None
    for attr_type, value in attrs:
        if attr_type == 0x0016:
            relay_endpoint = parse_xor_addr(value)
            break
    if relay_endpoint is None:
        raise RuntimeError("allocate response missing XOR-RELAYED-ADDRESS")

    result["relay_endpoint"] = {"host": relay_endpoint[0], "port": relay_endpoint[1]}
    write_remote_json(remote_target, ssh_key, remote_cmd_path, result["relay_endpoint"])

    hello_indication, _ = client.recvfrom(4096)
    msg_type, attrs = parse(hello_indication)
    if msg_type != 0x0007:
        raise RuntimeError(f"unexpected data indication: 0x{msg_type:04x}")

    peer_seen = None
    hello_payload = None
    for attr_type, value in attrs:
        if attr_type == 0x0012:
            peer_seen = parse_xor_addr(value)
        if attr_type == 0x0013:
            hello_payload = value.decode("utf-8", "replace")
    if peer_seen is None:
        raise RuntimeError("data indication missing XOR-PEER-ADDRESS")

    result["peer_seen_by_relay"] = {"host": peer_seen[0], "port": peer_seen[1]}
    result["hello_payload"] = hello_payload

    perm_tid = transaction_id()
    permission_request = stun(0x0008, perm_tid, [attr(0x0012, xor_addr(peer_seen[0], peer_seen[1]))])
    client.sendto(permission_request, (relay_host, turn_port))
    perm_resp, _ = client.recvfrom(2048)
    msg_type, _ = parse(perm_resp)
    if msg_type != 0x0108:
        raise RuntimeError(f"unexpected permission response: 0x{msg_type:04x}")
    result["permission_response"] = f"0x{msg_type:03x}"

    outbound = f"laptop-to-{peer_label}-via-aliyun-relay".encode("utf-8")
    send_tid = transaction_id()
    send_request = stun(
        0x0006,
        send_tid,
        [attr(0x0012, xor_addr(peer_seen[0], peer_seen[1])), attr(0x0013, outbound)],
    )
    client.sendto(send_request, (relay_host, turn_port))

    reply_indication, _ = client.recvfrom(4096)
    msg_type, attrs = parse(reply_indication)
    if msg_type != 0x0007:
        raise RuntimeError(f"unexpected reply indication: 0x{msg_type:04x}")

    reply_payload = None
    reply_peer = None
    for attr_type, value in attrs:
        if attr_type == 0x0012:
            reply_peer = parse_xor_addr(value)
        if attr_type == 0x0013:
            reply_payload = value.decode("utf-8", "replace")
    if reply_peer is None:
        raise RuntimeError("reply indication missing XOR-PEER-ADDRESS")

    result["reply_peer_seen"] = {"host": reply_peer[0], "port": reply_peer[1]}
    result["reply_payload_to_laptop"] = reply_payload

    for _ in range(20):
        state = read_remote_json(remote_target, ssh_key, remote_state_path)
        result["peer_state_final"] = state
        if state.get("status") in {"received", "replied"}:
            break
        time.sleep(0.5)

    print(json.dumps(result, ensure_ascii=False, indent=2))
except Exception as exc:
    print(f"ERROR: {exc}", file=sys.stderr)
    raise
finally:
    client.close()
PY
