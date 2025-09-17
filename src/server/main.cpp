#include <manager/config.hpp>
#include <manager/auth/routes.hpp>
#include <manager/users/routes.hpp>
#include <app/viewer/routes.hpp>
#include <app/memo/routes.hpp>

static_assert(CROW_ENABLE_SSL, "CROW_ENABLE_SSL is not defined");

int main(int argc, char* argv[]) {
	if (!CONFIG::load_params("config/param.json")) {
		std::cerr << "Failed to load configuration. Exiting." << std::endl;
		return 1;
	}

	crow::App<MIDDLEWARE::AuthMiddleware> app;
	app.port(CONFIG::params.SERVER_PORT);
	app.ssl_file(CONFIG::params.SSL_CERT_PATH, CONFIG::params.SSL_KEY_PATH);
	
	AUTH::setup(app);
	USER::setup(app);
	VIEWER::setup(app, std::move(CONFIG::params.VIEWER_DIR));
	MEMO::setup(app);
	
	app.multithreaded().run();
} 