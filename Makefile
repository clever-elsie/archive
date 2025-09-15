# パラメータファイルの読み込み（存在すれば上書き）
-include makefile.env

.PHONY: all run debug build debug_build stop reload see watch clean install

all: build Makefile

run: build
	./build/home-server

debug: debug_build
	./build/home-server

build:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j$(nproc)

debug_build:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
	cmake --build build -j$(nproc)

stop:
	sudo systemctl stop $(SERVICE)

reload: build
	sudo systemctl restart $(SERVICE)

see:
	systemctl status $(SERVICE)

watch:
	watch systemctl status $(SERVICE)

clean:
	rm -f build/home-server

install: build
	sudo systemctl daemon-reload
	sudo systemctl enable $(SERVICE)
	sudo systemctl start $(SERVICE)
