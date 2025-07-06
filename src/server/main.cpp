#include "server_systemd.hpp"
#include "config.hpp"

int main(int argc, char* argv[]) {
	if (!CONFIG::load_params("config/param.json")) {
		std::cerr << "Failed to load configuration. Exiting." << std::endl;
		return 1;
	}
	// ドメイン設定を初期化
	std::cout << "=== Domain Configuration ===" << std::endl;
	std::cout << "Config file: config/param.json" << std::endl;
	std::cout << "Allowed origins:" << std::endl;
	for (const auto& origin : CONFIG::get_allowed_origins()) {
		std::cout << "  - " << origin << std::endl;
	}
	std::cout << "===========================" << std::endl;
	
	// パスを動的に取得
	namespace fs = std::filesystem;
	comic::base_dir = fs::canonical(fs::current_path() / "data").string();
	comic::rel_base = fs::canonical(fs::current_path()).string()+'/';
	comic::base_time = fs::last_write_time(comic::base_dir);
	
	// memoのパスも設定
	MEMO::buf_path = fs::canonical(fs::current_path() / "memo" / "buf").string() + "/";
	
	crow::App<MIDDLEWARE::AuthMiddleware> app;
	app.port(CONFIG::params.SERVER_PORT);
#if defined(CROW_ENABLE_SSL)
	app.ssl_file(CONFIG::params.SSL_CERT_PATH, CONFIG::params.SSL_KEY_PATH);
#endif
	
	/*auth*/{
		CROW_ROUTE(app,"/req/auth/login")
			.methods(crow::HTTPMethod::POST)
				(AUTH::login_response);
		CROW_ROUTE(app,"/req/auth/logout")
			.methods(crow::HTTPMethod::POST)
				(AUTH::logout_response);
		CROW_ROUTE(app,"/req/auth/check")
			.methods(crow::HTTPMethod::POST)
				(AUTH::check_auth_response);
	}// auth end
	
	/*memo*/{
		CROW_ROUTE(app,"/req/memo/all")
			.methods(crow::HTTPMethod::GET)
				(MEMO::memo_fetch_all);
		CROW_ROUTE(app,"/req/memo/new_id")
			.methods(crow::HTTPMethod::GET)
				(MEMO::memo_issue_new_id);
		CROW_ROUTE(app,"/req/memo/renew")
			.methods(crow::HTTPMethod::POST)
				(MEMO::memo_renew);
		CROW_ROUTE(app,"/req/memo/now")
			.methods(crow::HTTPMethod::GET)
				(MEMO::memo_now);
		CROW_ROUTE(app,"/req/memo/remove")
			.methods(crow::HTTPMethod::POST)
				(MEMO::memo_rm);
	}// memo end
	
	/*user management*/{
		USER_ROUTES::setup_user_routes(app);
	}// user management end
	/*viewer*/{
		// 個数固定のランダムに選択された画像パスを返す
		CROW_ROUTE(app,"/req/img/rand")
			.methods(crow::HTTPMethod::GET)
				(comic::get_rand_imgs);
		CROW_ROUTE(app,"/req/img")
			.methods(crow::HTTPMethod::POST)
				(comic::get_imgs);
		CROW_ROUTE(app,"/req/img/creator")
			.methods(crow::HTTPMethod::POST)
				(comic::get_creators_all);
		CROW_ROUTE(app,"/req/img/page_list")
			.methods(crow::HTTPMethod::GET)
				(comic::get_page_list);
		CROW_ROUTE(app,"/req/img/page")
			.methods(crow::HTTPMethod::POST)
				(comic::get_page);
		CROW_ROUTE(app,"/req/img/retrieve")
			.methods(crow::HTTPMethod::POST)
				(comic::retrieve_query);
		CROW_ROUTE(app,"/req/img/dir_access")
			.methods(crow::HTTPMethod::POST)
				(comic::get_dir_list);
		// ファイル構造キャッシュリロード
		CROW_ROUTE(app,"/req/img/reload")
			.methods(crow::HTTPMethod::GET)
				(comic::reload_leaf);
		// タグ，作成者更新
		CROW_ROUTE(app,"/req/img/info_renew")
			.methods(crow::HTTPMethod::POST)
				(comic::info_renew);
	}// end comic
	comic::load_leaf_dir(comic::base_dir);
	app.multithreaded()
	.run();
} 