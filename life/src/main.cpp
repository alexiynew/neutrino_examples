#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <array>
#include <variant>
#include <filesystem>

#include <common/utils.hpp>
#include <graphics/color.hpp>
#include <graphics/font.hpp>
#include <graphics/mesh.hpp>
#include <graphics/renderer.hpp>
#include <graphics/shader.hpp>
#include <log/log.hpp>
#include <log/stream_logger.hpp>
#include <math/math.hpp>
#include <system/window.hpp>
#include <profiler/profiler.hpp>

namespace N = neutrino;
namespace NS = neutrino::system;
namespace NG = neutrino::graphics;
namespace NM = neutrino::math;
namespace NL = neutrino::log;
namespace NU = neutrino::utils;
namespace NP = neutrino::profiler;

namespace
{
    const std::filesystem::path fragment_shader = "data/fragment.frag";
    const std::filesystem::path particle_vertex_shader = "data/particle.vert";
    const std::filesystem::path text_vertex_shader = "data/text.vert";

    const NG::Mesh::VertexData vertices = {{-0.5, -0.5, 0.0}, {0.5, -0.5, 0.0}, {0.5, 0.5, 0.0}, {-0.5, 0.5, 0.0}};
    NG::Mesh::IndicesData indices = {0, 1, 2, 0, 2, 3};

    constexpr std::size_t EntityesCount = 100000;
}

struct RenderComponent
{
    NG::Colorf color;
};

struct SizeComponent
{
    NM::Vector2i size;
};

struct PositionComponent
{
    NM::Vector2i pos;
};

struct MovementComponent
{
    NM::Vector2i offset;
};

template <typename VariantType, typename T, std::size_t index = 0>
inline constexpr std::size_t get_component_index()
{
    static_assert(std::variant_size_v<VariantType> > index, "Type not found in variant");

    if constexpr (index == std::variant_size_v<VariantType>)
    {
        return index;
    }
    else if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantType>, T>)
    {
        return index;
    }
    else
    {
        return get_component_index<VariantType, T, index + 1>();
    }
}

template <typename VariantType, typename ComponentType>
static inline constexpr std::size_t component_index_v = get_component_index<VariantType, ComponentType>();

template <typename... Types>
inline constexpr auto wrap_in_variant(const Types &...)
{
    return std::variant<Types...>();
}

template <typename... Types>
inline constexpr auto wrap_in_variant_of_vectors(const Types &...)
{
    return wrap_in_variant(std::vector<Types>()...);
}

template <typename... Types>
using wrap_in_variant_of_vectors_t = decltype(wrap_in_variant_of_vectors(Types()...));

template <typename ECSType>
class System
{
public:
    virtual ~System() = default;

    void set_storage(ECSType *storage)
    {
        m_storage = storage;
    }

    const ECSType &storage() const
    {
        return *m_storage;
    }

    ECSType &storage()
    {
        return *m_storage;
    }

    virtual void update() = 0;

private:
    ECSType *m_storage;
};

template <typename... Types>
class ECS
{
public:
    template <typename T>
    using StorageContainer = std::vector<T>;
    using StorageItemType = wrap_in_variant_of_vectors_t<Types...>;
    using StorageType = std::array<StorageItemType, std::variant_size_v<StorageItemType>>;
    using SystemType = System<ECS<Types...>>;

    ECS()
    {
        std::size_t i = 0;
        (void(m_storage[i++] = StorageContainer<Types>()), ...);
    }

    template <typename ComponentType>
    void add_component(ComponentType component)
    {
        using ContainerType = StorageContainer<ComponentType>;
        static constexpr std::size_t index = component_index_v<StorageItemType, ContainerType>;
        std::get<ContainerType>(m_storage[index]).push_back(component);
    }

    void add_system(std::unique_ptr<SystemType> system)
    {
        system->set_storage(this);
        m_systems.push_back(std::move(system));
    }

    template <typename ComponentType>
    const ComponentType &component(std::size_t index) const
    {
        return components_container<ComponentType>()[index];
    }

    template <typename ComponentType>
    ComponentType &component(std::size_t index)
    {
        return components_container<ComponentType>()[index];
    }

    template <typename ComponentType>
    StorageContainer<ComponentType> &components_container()
    {
        using ContainerType = StorageContainer<ComponentType>;
        static constexpr std::size_t container_index = component_index_v<StorageItemType, ContainerType>;
        return std::get<ContainerType>(m_storage[container_index]);
    }

    template <typename ComponentType>
    const StorageContainer<ComponentType> &components_container() const
    {
        using ContainerType = StorageContainer<ComponentType>;
        static constexpr std::size_t container_index = component_index_v<StorageItemType, ContainerType>;
        return std::get<ContainerType>(m_storage[container_index]);
    }

    void update()
    {
        for (auto &s : m_systems)
        {
            s->update();
        }
    }

private:
    StorageType m_storage;
    std::vector<std::unique_ptr<SystemType>> m_systems;
};

using ECSType = ECS<RenderComponent, SizeComponent, PositionComponent, MovementComponent>;

class RenderSystem final : public ECSType::SystemType
{
public:
    RenderSystem(NG::Renderer &renderer, NG::Renderer::ResourceId mesh_id,
                 NG::Renderer::ResourceId shader_id)
        : m_renderer(renderer), m_mesh_id(mesh_id), m_shader_id(shader_id) {}

    void update() override
    {
        for (std::size_t index = 0; index < EntityesCount; ++index)
        {
            const auto &r = storage().component<RenderComponent>(index);
            const auto &s = storage().component<SizeComponent>(index);
            const auto &p = storage().component<PositionComponent>(index);

            m_renderer.render(m_mesh_id,
                              m_shader_id,
                              {NG::Uniform{"pos", NM::Vector3f{p.pos, 0}},
                               NG::Uniform{"size", NM::Vector3f{s.size, 1}},
                               NG::Uniform{"color", r.color}});
        }
    }

private:
    NG::Renderer &m_renderer;
    NG::Renderer::ResourceId m_mesh_id;
    NG::Renderer::ResourceId m_shader_id;
};

class MovementSystem : public ECSType::SystemType
{
public:
    MovementSystem(N::Size size)
        : m_size(size)
    {
    }

    void update() override
    {
        for (std::size_t index = 0; index < EntityesCount; ++index)
        {
            auto &p = storage().component<PositionComponent>(index);
            auto &s = storage().component<SizeComponent>(index);
            auto &m = storage().component<MovementComponent>(index);

            const auto &pos = p.pos;
            const auto &size = s.size;

            if (pos.x + size.x / 2 > m_size.width || pos.x - size.x / 2 < 0)
            {
                m.offset.x *= -1;
            }

            if (pos.y + size.y / 2 > m_size.height || pos.y - size.y / 2 < 0)
            {
                m.offset.y *= -1;
            }

            p.pos += m.offset;
        }
    }

private:
    N::Size m_size;
};

class App
{
public:
    App()
        : m_window("Life", {800, 600}), m_renderer(m_window.context())
    {
    }

    void init()
    {
        m_window.set_on_resize_callback([this](N::Size size)
                                        { on_resize(size); });

        for (std::size_t i = 0; i < EntityesCount; ++i)
        {
            const auto color = NU::random_numbers<float>(0.2f, 1.0f, 3);
            const auto size = NU::random_numbers<int>(10, 20, 2);
            const auto pos = NU::random_numbers<int>(100, 200, 2);
            const auto offset = NU::random_numbers<int>(-10, 10, 2);

            RenderComponent r{.color{color[0], color[1], color[1], 1.0f}};
            SizeComponent s{.size{size[0], size[1]}};
            PositionComponent p{.pos{pos[0], pos[1]}};
            MovementComponent m{.offset{offset[0], offset[1]}};

            m_ecs.add_component(r);
            m_ecs.add_component(s);
            m_ecs.add_component(p);
            m_ecs.add_component(m);
        }

        m_ecs.add_system(std::make_unique<RenderSystem>(m_renderer, m_mesh_id, m_particle_shader_id));
        m_ecs.add_system(std::make_unique<MovementSystem>(N::Size(800, 600)));

        if (m_font.load("data/UbuntuMono-Regular.ttf") != NG::Font::LoadResult::Success)
        {
            throw std::runtime_error("Can't load font.");
        }

        NG::Shader particle_shader;
        particle_shader.set_vertex_source(particle_vertex_shader);
        particle_shader.set_fragment_source(fragment_shader);
        if (!m_renderer.load(m_particle_shader_id, particle_shader))
        {
            throw std::runtime_error("Can't load shader.");
        }

        NG::Shader text_shader;
        text_shader.set_vertex_source(text_vertex_shader);
        text_shader.set_fragment_source(fragment_shader);
        if (!m_renderer.load(m_text_shader_id, text_shader))
        {
            throw std::runtime_error("Can't load shader.");
        }

        NG::Mesh mesh;
        mesh.set_vertices(vertices);
        mesh.add_submesh(indices);

        m_renderer.load(m_mesh_id, mesh);

        m_renderer.set_clear_color(NG::Color(0x2F2F2FFFU));
    }

    void run()
    {
        NP::begin_profiling("Life");
        m_window.show();

        m_frame_time = std::chrono::milliseconds(0);
        m_last_frame_time = std::chrono::steady_clock::now();

        while (!m_window.should_close())
        {
            auto s1 = NP::count_scope("loop");
            m_window.process_events();

            {
                auto s2 = NP::count_scope("update");
                m_ecs.update();
                render_fps();
            }

            {
                auto s3 = NP::count_scope("display");
                m_renderer.display();
            }

            tick();
        }

        NP::dump_to_file("Life.json");
    }

    void on_resize(N::Size size)
    {
        m_renderer.set_uniform("projectionMatrix",
                               NM::ortho2d<float>(0, static_cast<float>(size.width), 0, static_cast<float>(size.height)));
        m_renderer.set_viewport(size);
    }

    void tick()
    {
        const auto now = std::chrono::steady_clock::now();
        m_last_frame_duration = (now - m_last_frame_time);
        m_frame_time += m_last_frame_duration;
        m_last_frame_time = now;

        while (m_frame_time > std::chrono::seconds(1))
        {
            m_frame_time -= std::chrono::seconds(1);
            m_fps = m_current_fps;
            m_current_fps = 0;
        }

        m_current_fps++;
    }

    void render_fps()
    {
        constexpr NM::Vector3f FpsTextBottomRightOffset = {120, -50, 0};
        constexpr NM::Vector3f normal_text_scale = {15, 15, 1};
        const auto size = m_window.size();

        // back
        m_renderer.render(m_mesh_id,
                          m_text_shader_id,
                          {NG::Uniform{"pos", NM::Vector3f{size.width - 80, 55, 0.1}},
                           NG::Uniform{"size", NM::Vector3f{100, 20, 1}},
                           NG::Uniform{"color", NG::Color(0x020202FFU)}});

        // text
        NM::Vector3f text_pos = NM::Vector3f{size.width, 0, 0.15} - FpsTextBottomRightOffset;

        std::string text = std::to_string(m_fps) + " " + std::to_string(m_last_frame_duration.count());

        m_renderer.load(m_text_id, m_font.create_text_mesh(text));
        m_renderer.render(m_text_id,
                          m_text_shader_id,
                          {NG::Uniform{"pos", text_pos},
                           NG::Uniform{"size", normal_text_scale},
                           NG::Uniform{"color", NM::Vector4f(0.9f, 0.5f, 0.6f, 1.0f)}});
    }

private:
    NS::Window m_window;
    NG::Renderer m_renderer;

    ECSType m_ecs;

    NG::Renderer::ResourceId m_particle_shader_id = 1;
    NG::Renderer::ResourceId m_text_shader_id = 2;

    NG::Renderer::ResourceId m_mesh_id = 1;
    NG::Renderer::ResourceId m_text_id = 2;

    int m_fps = 0;
    int m_current_fps = 0;
    std::chrono::steady_clock::duration m_frame_time;
    std::chrono::steady_clock::duration m_last_frame_duration;
    std::chrono::steady_clock::time_point m_last_frame_time;

    NG::Font m_font;
};

int main()
{
    neutrino::log::set_logger(std::make_unique<NL::StreamLogger>(std::cout));
    NL::info("Main") << "RUN";

    App app;
    app.init();
    app.run();
}
