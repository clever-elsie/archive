# デフォルト値の定義
SERVICE ?= myservice
OUT ?= server_systemd.out
# viewer のベースディレクトリ
# 空の場合はカレントからの data を使用（従来動作）
VIEWER_DIR ?=

# パラメータファイルの読み込み（存在すれば上書き）
-include makefile.env

CC = g++ -std=gnu++2b -O2 -I /usr/local/include
OPT=-lssl -lcrypto
SRCDIR = src/server
target = $(SRCDIR)/main.cpp

HDRS:=$(shell find $(SRCDIR) -type f -name "*.hpp")

all: $(OUT) Makefile

run: all
	./$(OUT)

build:
	if [[ -f $(SSL_CERT) && -f $(SSL_KEY) && -f $(NGINX_CONFIG) ]]; then
		echo "[SUCCESS] All files for nginx are found"
	else
		if [ ! -f $(SSL_CERT) ]; then
			echo "[ERROR] $(SSL_CERT) not found"
		fi
		if [ ! -f $(SSL_KEY) ]; then
			echo "[ERROR] $(SSL_KEY) not found"
		fi
		if [ ! -f $(NGINX_CONFIG) ]; then
			echo "[ERROR] $(NGINX_CONFIG) not found"
		fi
		exit 1
	fi
	if [ ! -f /etc/systemd/system/$(SERVICE).service ]; then
		echo "[ERROR] $(SERVICE).service not found"
		exit 1
	fi
	if [ ! -f config/param.json ]; then
		echo "[ERROR] config/param.json not found"
		exit 1
	fi
	if [ ! -f users.json ]; then
		echo "[WARN] users.json not found. Creating..."
		touch users.json
	fi
	make all && make install

$(OUT): $(target) Makefile $(HDRS)
	$(CC) $(target) -o $(OUT) $(OPT) -DVIEWER_DIR=\"$(VIEWER_DIR)\"
	
stop:
	sudo systemctl stop $(SERVICE)

reload: $(OUT)
	sudo systemctl restart $(SERVICE)

see:
	systemctl status $(SERVICE)

watch:
	watch systemctl status $(SERVICE)

clean:
	rm -f $(OUT)
	rm -f *.o

install: $(OUT)
	sudo systemctl daemon-reload
	sudo systemctl enable $(SERVICE)
	sudo systemctl start $(SERVICE)
