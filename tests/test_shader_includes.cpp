#include "test_framework.hpp"
#include "tests.hpp"

#include <fuhgl/gl/shader.hpp>

#include <string>

using fuhgl::Shader;

void runShaderIncludeTests() {
    // Resolver with a tiny virtual file system.
    auto resolver = [](const std::string& path) -> std::string {
        if (path == "common.glsl") return "float pi = 3.14;\n#include \"inner.glsl\"\n";
        if (path == "inner.glsl") return "float two = 2.0;\n";
        return {};
    };

    const std::string src = "#version 330 core\n#include \"common.glsl\"\nvoid main() {}\n";
    const std::string out = Shader::resolveIncludes(src, resolver);

    // #version stays the first line
    CHECK(out.rfind("#version 330 core", 0) == 0);
    // includes expanded, recursively
    CHECK(out.find("float pi = 3.14;") != std::string::npos);
    CHECK(out.find("float two = 2.0;") != std::string::npos);
    CHECK(out.find("#include") == std::string::npos);
    // missing include -> resolver returns empty, line dropped
    const std::string out2 = Shader::resolveIncludes("#version 330 core\n#include \"missing.glsl\"\nvoid main(){}\n", resolver);
    CHECK(out2.find("missing.glsl") == std::string::npos);
    CHECK(out2.find("void main(){}") != std::string::npos);
    // null resolver / depth guard: source passes through unchanged
    CHECK(Shader::resolveIncludes(src, Shader::IncludeResolver{}) == src);

    // hardening: a null source string is treated as empty (no GL context here ->
    // the shader was never created, so compile() must simply return false)
    Shader s;
    CHECK(!s.compile(nullptr));

    // hardening: a self-including resolver terminates via the depth guard
    auto selfIncl = [](const std::string& path) -> std::string {
        return path == "loop.glsl" ? "#include \"loop.glsl\"\n" : std::string{};
    };
    const std::string loopOut = Shader::resolveIncludes("#include \"loop.glsl\"\nvoid main(){}\n", selfIncl);
    CHECK(loopOut.find("void main(){}") != std::string::npos);
    CHECK(loopOut.size() < 1024);

    // hardening: the resolved output is size-capped even if the resolver returns
    // a huge payload (4 MiB cap + the pass-through lines of the input)
    auto big = [](const std::string&) -> std::string { return std::string(8u << 20, 'x'); };
    const std::string bigOut = Shader::resolveIncludes("#include \"big.glsl\"\nvoid main(){}\n", big);
    CHECK(bigOut.size() < (std::size_t{5} << 20));
    CHECK(bigOut.find("void main(){}") != std::string::npos);
}
