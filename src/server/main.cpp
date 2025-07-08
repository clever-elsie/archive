#define CROW_ENABLE_SSL
#include "headers.hpp"
#include "manager/auth/auth_routes.hpp"
#include "manager/users/user_routes.hpp"
#include "manager/config.hpp"
#include "app/viewer/viewer_routes.hpp"
#include "app/memo/memo_routes.hpp"

int main(int argc, char* argv[]) {
	if (!CONFIG::load_params("config/param.json")) {
		std::cerr << "Failed to load configuration. Exiting." << std::endl;
		return 1;
	}

	crow::App<MIDDLEWARE::AuthMiddleware> app;
	app.port(CONFIG::params.SERVER_PORT);
#if defined(CROW_ENABLE_SSL)
	app.ssl_file(CONFIG::params.SSL_CERT_PATH, CONFIG::params.SSL_KEY_PATH);
#endif
	
	AUTH::setup_auth_routes(app);
	USER_ROUTES::setup_user_routes(app);
	VIEWER_ROUTES::setup_viewer_routes(app);
	MEMO_ROUTES::setup_memo_routes(app);
	
	app.multithreaded()
	.run();
} 