#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include <system/window.hpp>

using namespace neutrino;
using namespace neutrino::system;

class App
{
public:
    App()
        : m_window("Life", {800, 600})
    {
    }

    void run()
    {
        m_window.show();
        while (!m_window.should_close())
        {
            m_window.process_events();
        }
    }

private:
    Window m_window;
};

int main()
{
    App app;
    app.run();
}
