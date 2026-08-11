#include <chrono>
#include <thread>
#include <manager/config.hpp>
#include <manager/auth/routes.hpp>
#include <manager/auth/authorization_middleware.hpp>
#include <manager/users/routes.hpp>
#include <app/viewer/routes.hpp>
#include <app/memo/routes.hpp>
#include <app/viewer/manager.hpp>
#include <manager/users/manager.hpp>

#include <csignal>

static_assert(CROW_ENABLE_SSL, "CROW_ENABLE_SSL is not defined");

static void handle_signal(int) {
	VIEWER::manager::get_instance().request_stop();
}

int main(int argc, char* argv[]) {
	std::signal(SIGINT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	const std::string config_path = CONFIG::config_path_from_args(argc, argv);
	if (config_path.empty() || !CONFIG::load_params(config_path)) {
		std::cerr << "Failed to load configuration. Exiting." << std::endl;
		return 1;
	}
	if (!USER_MANAGER::get_user_manager().initialize(CONFIG::params.USER_STORE_PATH)) {
		std::cerr << "Failed to initialize user store. Exiting." << std::endl;
		return 1;
	}

	crow::App<MIDDLEWARE::AuthMiddleware, MIDDLEWARE::AuthorizationMiddleware> app;
	app.port(CONFIG::params.SERVER_PORT);
	app.ssl_file(CONFIG::params.SSL_CERT_PATH, CONFIG::params.SSL_KEY_PATH);
	
	AUTH::setup(app);
	USER::setup(app);
	VIEWER::setup(app, std::move(CONFIG::params.VIEWER_DIR));
	MEMO::setup(app);
	
	const unsigned hwc = std::thread::hardware_concurrency();
	constexpr unsigned low = 4u;
	app.concurrency(std::min(low, hwc));
	auto server = app.run_async();
	if (app.wait_for_server_start(std::chrono::seconds{30}) == std::cv_status::no_timeout)
		VIEWER::manager::get_instance().start_initial_load();
	server.get();
	VIEWER::manager::get_instance().shutdown(); // 終了前にバックグラウンドスレッドを停止
} 
