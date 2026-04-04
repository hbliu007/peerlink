#!/bin/bash
# PeerLink阿里云服务端部署脚本
# 服务器: <YOUR_SERVER_IP>
# 用户: <YOUR_USER>

set -e

REMOTE_HOST="${PEERLINK_SERVER_IP:?请设置环境变量 PEERLINK_SERVER_IP}"
REMOTE_USER="${PEERLINK_SERVER_USER:-root}"
DEPLOY_DIR="/opt/peerlink"
CONFIG_DIR="/etc/peerlink"

echo "=== PeerLink阿里云部署脚本 ==="
echo "目标服务器: ${REMOTE_USER}@${REMOTE_HOST}"
echo "部署目录: ${DEPLOY_DIR}"
echo ""

# 检查本地二进制文件
echo "检查本地编译的二进制文件..."
if [ ! -f "build/src/servers/stun/stun_server" ]; then
    echo "错误: 找不到 stun_server，请先运行 ./build.sh"
    exit 1
fi

# 创建临时部署包
echo "创建部署包..."
TEMP_DIR=$(mktemp -d)
mkdir -p ${TEMP_DIR}/bin
mkdir -p ${TEMP_DIR}/config
mkdir -p ${TEMP_DIR}/deploy

# 复制二进制文件
cp build/src/servers/stun/stun_server ${TEMP_DIR}/bin/
cp build/src/servers/relay/relay_server ${TEMP_DIR}/bin/
cp build/src/servers/did/did-server ${TEMP_DIR}/bin/
cp build/src/servers/signaling/p2p-signaling-server ${TEMP_DIR}/bin/

# 复制配置文件
cp config/peerlink.aliyun.example.toml ${TEMP_DIR}/config/peerlink.toml
cp -r deploy/systemd ${TEMP_DIR}/deploy/
cp -r deploy/nginx ${TEMP_DIR}/deploy/

# 打包
cd ${TEMP_DIR}
tar czf /tmp/peerlink-deploy.tar.gz .
cd -

echo "部署包已创建: /tmp/peerlink-deploy.tar.gz"
echo ""
echo "请手动执行以下步骤部署到阿里云:"
echo ""
echo "1. 上传部署包:"
echo "   scp /tmp/peerlink-deploy.tar.gz root@${REMOTE_HOST}:/tmp/"
echo ""
echo "2. SSH登录服务器:"
echo "   ssh root@${REMOTE_HOST}"
echo ""
echo "3. 在服务器上执行:"
echo "   cd /tmp"
echo "   tar xzf peerlink-deploy.tar.gz -C ${DEPLOY_DIR}"
echo "   cp ${DEPLOY_DIR}/config/peerlink.toml ${CONFIG_DIR}/"
echo "   # 编辑配置文件，修改JWT_SECRET和public_ip"
echo "   vi ${CONFIG_DIR}/peerlink.toml"
echo "   # 安装systemd服务"
echo "   cp ${DEPLOY_DIR}/deploy/systemd/*.service /etc/systemd/system/"
echo "   systemctl daemon-reload"
echo "   systemctl enable peerlink-stun peerlink-relay peerlink-signaling peerlink-did"
echo "   systemctl start peerlink-stun peerlink-relay peerlink-signaling peerlink-did"
echo "   # 检查服务状态"
echo "   systemctl status peerlink-*"
echo ""

# 清理
rm -rf ${TEMP_DIR}

echo "部署脚本完成！"
