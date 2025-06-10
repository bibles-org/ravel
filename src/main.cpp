#include <app/application.h>

int main() {
    auto app_instance = app::application::create(1600, 900, "ravel");
    if (!app_instance) {
        return -1;
    }

    app_instance->run();

    return 0;
}
