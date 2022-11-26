#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <array>
#include <variant>

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
#include <common/fps_counter.hpp>

namespace N = neutrino;
namespace NS = neutrino::system;
namespace NG = neutrino::graphics;
namespace NM = neutrino::math;
namespace NL = neutrino::log;
namespace NU = neutrino::utils;

namespace
{
    const std::string vertex_shader =
        "#version 330 core\n\
\n\
layout(location = 0) in vec3 position;\n\
\n\
uniform mat4 modelMatrix;\n\
uniform mat4 projectionMatrix;\n\
uniform vec4 color;\n\
\n\
out vec4 fragColor;\n\
\n\
void main()\n\
{\n\
    gl_Position = projectionMatrix * modelMatrix * vec4(position, 1.0);\n\
    fragColor = color;\n\
}\n\
";

    const std::string fragment_shader =
        "#version 330 core\n\
\n\
in vec4 fragColor;\n\
\n\
out vec4 color;\n\
\n\
void main()\n\
{\n\
    color = fragColor;\n\
}\n\
";

    const NG::Mesh::VertexData vertices = {{-0.5, -0.5, 0.0}, {0.5, -0.5, 0.0}, {0.5, 0.5, 0.0}, {-0.5, 0.5, 0.0}};
    NG::Mesh::IndicesData indices = {0, 1, 2, 0, 2, 3};
}

struct RenderComponent
{
    NG::Colorf color;
};

struct SizeComponent
{
    int width = 0;
    int height = 0;
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
        using ContainerType = StorageContainer<ComponentType>;
        static constexpr std::size_t container_index = component_index_v<StorageItemType, ContainerType>;
        return std::get<ContainerType>(m_storage[container_index])[index];
    }

    template <typename ComponentType>
    ComponentType &component(std::size_t index)
    {
        using ContainerType = StorageContainer<ComponentType>;
        static constexpr std::size_t container_index = component_index_v<StorageItemType, ContainerType>;
        return std::get<ContainerType>(m_storage[container_index])[index];
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

constexpr std::size_t EntityesCount = 1;

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

            const NM::Vector3f pos(p.pos.x, p.pos.y, 0.1);
            const NM::Matrix4f transform = scale(translate(NM::Matrix4f(), pos), {s.width, s.height, 1});

            m_renderer.render(m_mesh_id,
                              m_shader_id,
                              {NG::Uniform{"modelMatrix", transform}, NG::Uniform{"color", r.color}});
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

            if (p.pos.x + s.width / 2 > m_size.width || p.pos.x - s.width / 2 < 0)
            {
                m.offset.x *= -1;
            }

            if (p.pos.y + s.height / 2 > m_size.height || p.pos.y - s.height / 2 < 0)
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
            SizeComponent s{.width = size[0], .height = size[1]};
            PositionComponent p{.pos{pos[0], pos[1]}};
            MovementComponent m{.offset{offset[0], offset[1]}};

            m_ecs.add_component(r);
            m_ecs.add_component(s);
            m_ecs.add_component(p);
            m_ecs.add_component(m);
        }

        m_ecs.add_system(std::make_unique<RenderSystem>(m_renderer, m_mesh_id, m_shader_id));
        m_ecs.add_system(std::make_unique<MovementSystem>(N::Size(800, 600)));

        if (m_font.load("data/UbuntuMono-Regular.ttf") != NG::Font::LoadResult::Success)
        {
            throw std::runtime_error("Can't load font.");
        }

        NG::Shader shader;
        shader.set_vertex_source(vertex_shader);
        shader.set_fragment_source(fragment_shader);
        if (!m_renderer.load(m_shader_id, shader))
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
        m_window.show();
        while (!m_window.should_close())
        {
            m_window.process_events();

            m_ecs.update();

            render_fps();

            m_renderer.display();

            m_fps.tick();
        }
    }

    void on_resize(N::Size size)
    {
        m_renderer.set_uniform("projectionMatrix",
                               NM::ortho2d<float>(0, static_cast<float>(size.width), 0, static_cast<float>(size.height)));
        m_renderer.set_viewport(size);
    }

    void render_fps()
    {
        constexpr NM::Vector3f FpsTextBottomRightOffset = {120, -50, 0};
        constexpr NM::Vector3f normal_text_scale = {15, 15, 1};

        const auto size = m_window.size();
        NM::Vector3f text_pos = NM::Vector3f{size.width, 0, 0} - FpsTextBottomRightOffset;

        m_renderer.load(m_text_id, m_font.create_text_mesh(std::to_string(m_fps.fps())));

        const NM::Matrix4f transform = scale(translate(NM::Matrix4f(), text_pos), normal_text_scale);
        m_renderer.render(m_text_id,
                          m_shader_id,
                          {NG::Uniform{"modelMatrix", transform}, NG::Uniform{"color", NM::Vector4f(0.9f, 0.5f, 0.6f, 1.0f)}});
    }

private:
    NS::Window m_window;
    NG::Renderer m_renderer;

    ECSType m_ecs;

    NG::Renderer::ResourceId m_shader_id = 1;

    NG::Renderer::ResourceId m_mesh_id = 1;
    NG::Renderer::ResourceId m_text_id = 2;

    N::FpsCounter m_fps;

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
