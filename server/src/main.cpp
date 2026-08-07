#include <thread>
#include <manager/config.hpp>
#include <manager/auth/routes.hpp>
#include <manager/auth/authorization_middleware.hpp>
#include <manager/users/routes.hpp>
#include <app/viewer/routes.hpp>
#include <app/memo/routes.hpp>
#include <app/viewer/manager.hpp>

#include <csignal>

static_assert(CROW_ENABLE_SSL, "CROW_ENABLE_SSL is not defined");

static void handle_signal(int) {
	VIEWER::manager::get_instance().request_stop();
}

int main(int argc, char* argv[]) {
	std::signal(SIGINT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	if (!CONFIG::load_params("config/param.json")) {
		std::cerr << "Failed to load configuration. Exiting." << std::endl;
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
	app.concurrency(std::min(low, hwc)).run();
	VIEWER::manager::get_instance().shutdown(); // 終了前にバックグラウンドスレッドを停止
} 
