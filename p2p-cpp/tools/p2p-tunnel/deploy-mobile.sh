#!/bin/bash
# 手机端部署脚本 - 在Termux中执行

set -e

echo "=== P2P Tunnel 手机端部署脚本 ==="
echo ""

# 1. 复制源代码
echo "[1/5] 复制源代码到Termux..."
cp /sdcard/Download/p2p-cpp-minimal.tar.gz ~/
cd ~
tar xzf p2p-cpp-minimal.tar.gz
echo "✓ 源代码已解压"

# 2. 安装依赖
echo ""
echo "[2/5] 安装编译依赖..."
pkg update -y
pkg install -y cmake clang boost openssl git
echo "✓ 依赖已安装"

# 3. 编译
echo ""
echo "[3/5] 编译P2P隧道客户端..."
cd ~/p2p-cpp
mkdir -p build
cd build
cmake .. -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_SERVERS=OFF
make p2p-tunnel-client -j4
echo "✓ 编译完成"

# 4. 检查可执行文件
echo ""
echo "[4/5] 检查可执行文件..."
ls -lh tools/p2p-tunnel/p2p-tunnel-client
file tools/p2p-tunnel/p2p-tunnel-client
echo "✓ 可执行文件已生成"

# 5. 创建启动脚本
echo ""
echo "[5/5] 创建启动脚本..."
cat > ~/start-p2p-tunnel.sh << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash
cd ~/p2p-cpp/build/tools/p2p-tunnel
./p2p-tunnel-client mobile-001 office-001 9000 8080 \
  ws://<YOUR_SERVER_IP>:8080 <YOUR_SERVER_IP>:3478
EOF
chmod +x ~/start-p2p-tunnel.sh
echo "✓ 启动脚本已创建: ~/start-p2p-tunnel.sh"

echo ""
echo "=== 部署完成 ==="
echo ""
echo "启动客户端："
echo "  ~/start-p2p-tunnel.sh"
echo ""
echo "或手动启动："
echo "  cd ~/p2p-cpp/build/tools/p2p-tunnel"
echo "  ./p2p-tunnel-client mobile-001 office-001 9000 8080 \\"
echo "    ws://<YOUR_SERVER_IP>:8080 <YOUR_SERVER_IP>:3478"
