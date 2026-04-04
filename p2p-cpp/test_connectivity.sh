#!/bin/bash
# PeerLink 网络连通性测试脚本

set -e

echo "=== PeerLink 网络连通性测试 ==="
echo ""

# 测试目标服务器
ALIYUN_SERVER="<YOUR_SERVER_IP>"
SERVER_213="<OFFICE_SERVER_1>"
SERVER_215="<OFFICE_SERVER_2>"

# 颜色定义
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

test_ssh() {
    local host=$1
    local user=$2
    echo -n "测试SSH连接 ${user}@${host}... "

    if timeout 5 ssh -o ConnectTimeout=3 -o BatchMode=yes ${user}@${host} "echo OK" &>/dev/null; then
        echo -e "${GREEN}✓ 成功${NC}"
        return 0
    else
        echo -e "${RED}✗ 失败${NC}"
        return 1
    fi
}

test_port() {
    local host=$1
    local port=$2
    local protocol=$3
    echo -n "测试端口 ${host}:${port} (${protocol})... "

    if [ "$protocol" = "tcp" ]; then
        if timeout 3 nc -zv ${host} ${port} &>/dev/null; then
            echo -e "${GREEN}✓ 开放${NC}"
            return 0
        else
            echo -e "${RED}✗ 关闭${NC}"
            return 1
        fi
    elif [ "$protocol" = "udp" ]; then
        if timeout 3 nc -zuv ${host} ${port} &>/dev/null; then
            echo -e "${GREEN}✓ 开放${NC}"
            return 0
        else
            echo -e "${YELLOW}? 无法确定${NC} (UDP测试不可靠)"
            return 1
        fi
    fi
}

test_ping() {
    local host=$1
    echo -n "测试ICMP ping ${host}... "

    if ping -c 1 -W 2 ${host} &>/dev/null; then
        echo -e "${GREEN}✓ 可达${NC}"
        return 0
    else
        echo -e "${RED}✗ 不可达${NC}"
        return 1
    fi
}

echo "1. 测试SSH连接"
echo "----------------------------------------"
test_ssh ${ALIYUN_SERVER} root || echo "   提示: 阿里云服务器SSH可能需要密钥认证"
test_ssh ${SERVER_213} lhb || echo "   提示: 213服务器可能需要VPN或在办公室网络"
test_ssh ${SERVER_215} lhb || echo "   提示: 215服务器可能需要VPN或在办公室网络"
echo ""

echo "2. 测试阿里云服务端口"
echo "----------------------------------------"
test_ping ${ALIYUN_SERVER}
test_port ${ALIYUN_SERVER} 22 tcp    # SSH
test_port ${ALIYUN_SERVER} 8080 tcp  # Signaling WebSocket
test_port ${ALIYUN_SERVER} 8081 tcp  # DID HTTP API
test_port ${ALIYUN_SERVER} 3478 udp  # STUN
test_port ${ALIYUN_SERVER} 3478 tcp  # STUN TCP
test_port ${ALIYUN_SERVER} 9001 udp  # TURN/Relay
echo ""

echo "3. 测试办公室服务器"
echo "----------------------------------------"
test_ping ${SERVER_213} || echo "   提示: 可能需要VPN连接"
test_ping ${SERVER_215} || echo "   提示: 可能需要VPN连接"
echo ""

echo "4. 网络诊断建议"
echo "----------------------------------------"
if ! test_ssh ${SERVER_213} lhb &>/dev/null && ! test_ssh ${SERVER_215} lhb &>/dev/null; then
    echo -e "${YELLOW}⚠ 办公室服务器不可达${NC}"
    echo "  可能原因:"
    echo "  1. 未连接VPN"
    echo "  2. 不在办公室网络"
    echo "  3. 服务器关机或网络故障"
    echo ""
    echo "  解决方案:"
    echo "  - 连接公司VPN后重试"
    echo "  - 在办公室网络环境下执行部署"
    echo "  - 联系IT检查服务器状态"
fi

if ! test_port ${ALIYUN_SERVER} 8080 tcp &>/dev/null; then
    echo -e "${YELLOW}⚠ 阿里云信令服务端口未开放${NC}"
    echo "  可能原因:"
    echo "  1. 服务未启动"
    echo "  2. 防火墙阻止"
    echo "  3. 安全组规则未配置"
    echo ""
    echo "  解决方案:"
    echo "  - 检查服务状态: systemctl status peerlink-signaling"
    echo "  - 检查防火墙: iptables -L -n | grep 8080"
    echo "  - 配置阿里云安全组规则"
fi

echo ""
echo "测试完成！"
