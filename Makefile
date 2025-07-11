# デフォルト値の定義
SERVICE ?= myservice
OUT ?= server_systemd.out

# パラメータファイルの読み込み（存在すれば上書き）
-include makefile.env

CC = g++ -std=gnu++2b -O2 -I /usr/local/include
OPT=-lssl -lcrypto
SRCDIR = src/server
target = $(SRCDIR)/main.cpp

MEMO_HDRS=$(SRCDIR)/app/memo/memo.hpp $(SRCDIR)/app/memo/memo_routes.hpp
VIEWER_HDRS=$(SRCDIR)/app/viewer/viewer.hpp $(SRCDIR)/app/viewer/viewer_routes.hpp
APP_HDRS=$(MEMO_HDRS) $(VIEWER_HDRS)

CONFIG_HDRS=$(SRCDIR)/manager/config.hpp
AUTH_HDRS=$(SRCDIR)/manager/auth/auth.hpp $(SRCDIR)/manager/auth/auth_routes.hpp $(SRCDIR)/manager/auth/middleware.hpp $(SRCDIR)/manager/auth/jwt.hpp
USER_HDRS=$(SRCDIR)/manager/users/user_manager.hpp $(SRCDIR)/manager/users/user_api.hpp $(SRCDIR)/manager/users/user_routes.hpp
MANAGER_HDRS=$(CONFIG_HDRS) $(AUTH_HDRS) $(USER_HDRS)

HDRS=$(APP_HDRS) $(MANAGER_HDRS)

all: $(OUT) Makefile

run: all
	./$(OUT)

$(OUT): $(target) Makefile $(HDRS)
	$(CC) $(target) -o $(OUT) $(OPT)
	
reload: $(OUT)
	sudo systemctl restart $(SERVICE)

see:
	systemctl status $(SERVICE)
watch:
	watch systemctl status $(SERVICE)

clean:
	rm -f $(OUT)
	rm -f *.o

# CI/CD関連のターゲット（Docker環境）
test-ci-local:
	@echo "Running Docker-based CI/CD tests..."
	@chmod +x scripts/docker-ci-runner.sh
	@./scripts/docker-ci-runner.sh

test-ci-local-quick:
	@echo "Running quick Docker-based CI/CD tests..."
	@chmod +x scripts/docker-ci-runner.sh
	@./scripts/docker-ci-runner.sh --quick

test-ci-local-build:
	@echo "Building and running Docker-based CI/CD tests..."
	@chmod +x scripts/docker-ci-runner.sh
	@./scripts/docker-ci-runner.sh --build

test-ci-local-verbose:
	@echo "Running verbose Docker-based CI/CD tests..."
	@chmod +x scripts/docker-ci-runner.sh
	@./scripts/docker-ci-runner.sh --verbose

# ホスト環境でのテスト（非推奨）
test-ci-host:
	@echo "Running host-based CI/CD tests (not recommended for production)..."
	@chmod +x scripts/test-ci-local.sh
	@./scripts/test-ci-local.sh --skip-docker --skip-security

test-ci-act:
	@echo "Running GitHub Actions locally with act..."
	@chmod +x scripts/test-ci-local.sh
	@./scripts/test-ci-local.sh --act

test-ci-act-quick:
	@echo "Running quick GitHub Actions locally with act..."
	@chmod +x scripts/test-ci-local.sh
	@./scripts/test-ci-local.sh --act --skip-docker --skip-security

# 個別のテストターゲット
test-makefile:
	@echo "Testing Makefile build..."
	@make clean
	@make all
	@echo "✓ Makefile build successful"

test-cmake:
	@echo "Testing CMake build..."
	@rm -rf build
	@mkdir -p build
	@cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)
	@echo "✓ CMake build successful"

test-docker:
	@echo "Testing Docker build..."
	@docker build -t archive:test .
	@echo "✓ Docker build successful"

test-docs:
	@echo "Testing documentation..."
	@[ -f "README.md" ] || (echo "README.md not found" && exit 1)
	@grep -q "## 主な機能" README.md || echo "Warning: Missing main features section"
	@grep -q "## セットアップ" README.md || echo "Warning: Missing setup section"
	@echo "✓ Documentation check completed"

install: $(OUT)
	sudo cp $(OUT) /usr/local/bin/
	sudo systemctl daemon-reload
	sudo systemctl enable $(SERVICE)
	sudo systemctl start $(SERVICE)
