#!/bin/bash
# PeerLink远程服务器部署脚本
# 支持在远程Linux服务器上编译和部署

set -e

if [ $# -lt 2 ]; then
    echo "用法: $0 <服务器地址> <部署类型>"
    echo "部署类型: server (服务端) 或 client (客户端)"
    echo ""
    echo "示例:"
    echo "  $0 <user>@<office-server-1> client"
    echo "  $0 <user>@<office-server-2> client"
    echo "  $0 <user>@<cloud-server-ip> server"
    exit 1
fi

REMOTE_HOST=$1
DEPLOY_TYPE=$2
REMOTE_DIR="/tmp/peerlink-deploy-$$"

echo "=== PeerLink远程部署脚本 ==="
echo "目标服务器: ${REMOTE_HOST}"
echo "部署类型: ${DEPLOY_TYPE}"
echo ""

# 创建源码包
echo "创建源码包..."
tar czf /tmp/peerlink-src.tar.gz \
    --exclude='.git' \
    --exclude='build*' \
    --exclude='*.o' \
    --exclude='*.a' \
    --exclude='.worktrees' \
    CMakeLists.txt src/ include/ proto/ examples/ config/ deploy/ third_party/ tests/

echo "上传源码包到服务器..."
ssh ${REMOTE_HOST} "mkdir -p ${REMOTE_DIR}"
scp /tmp/peerlink-src.tar.gz ${REMOTE_HOST}:${REMOTE_DIR}/

if [ "${DEPLOY_TYPE}" = "server" ]; then
    echo "在服务器上编译服务端..."
    ssh ${REMOTE_HOST} "cd ${REMOTE_DIR} && \
        tar xzf peerlink-src.tar.gz && \
        cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_SERVERS=ON -DBUILD_SHARED_LIBS=OFF && \
        cmake --build build --target stun_server relay_server p2p-signaling-server did-server -j\$(nproc) && \
        echo '编译完成，二进制文件位于:' && \
        ls -lh build/src/servers/*/stun_server build/src/servers/*/relay_server build/src/servers/*/p2p-signaling-server build/src/servers/*/did-server 2>/dev/null"

    echo ""
    echo "服务端编译完成！"
    echo "二进制文件位于服务器: ${REMOTE_DIR}/build/src/servers/"
    echo ""
    echo "请SSH登录服务器继续部署:"
    echo "  ssh ${REMOTE_HOST}"
    echo "  cd ${REMOTE_DIR}"
    echo "  sudo mkdir -p /opt/peerlink/bin /etc/peerlink"
    echo "  sudo cp build/src/servers/stun/stun_server /opt/peerlink/bin/"
    echo "  sudo cp build/src/servers/relay/relay_server /opt/peerlink/bin/"
    echo "  sudo cp build/src/servers/signaling/p2p-signaling-server /opt/peerlink/bin/"
    echo "  sudo cp build/src/servers/did/did-server /opt/peerlink/bin/"
    echo "  sudo cp config/peerlink.aliyun.example.toml /etc/peerlink/peerlink.toml"
    echo "  # 编辑配置文件"
    echo "  sudo vi /etc/peerlink/peerlink.toml"

elif [ "${DEPLOY_TYPE}" = "client" ]; then
    echo "在服务器上编译客户端..."
    ssh ${REMOTE_HOST} "cd ${REMOTE_DIR} && \
        tar xzf peerlink-src.tar.gz && \
        cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=ON -DBUILD_SHARED_LIBS=OFF && \
        cmake --build build --target basic_client -j\$(nproc) && \
        echo '编译完成，客户端示例位于:' && \
        ls -lh build/examples/basic/basic_client 2>/dev/null"

    echo ""
    echo "客户端编译完成！"
    echo "客户端示例位于服务器: ${REMOTE_DIR}/build/examples/basic/basic_client"
    echo ""
    echo "测试客户端连接:"
    echo "  ssh ${REMOTE_HOST}"
    echo "  cd ${REMOTE_DIR}"
    echo "  ./build/examples/basic/basic_client <my_did> <peer_did>"
else
    echo "错误: 未知的部署类型 '${DEPLOY_TYPE}'"
    exit 1
fi

echo ""
echo "部署脚本完成！"
