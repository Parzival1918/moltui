class Moltui < Formula
  desc "High-performance 3D molecular and crystal structure viewer for the terminal"
  homepage "https://parzival1918.github.io/moltui/"
  url "https://github.com/Parzival1918/moltui/archive/refs/tags/v0.1.0.tar.gz"
  # Update with: curl -sL <url> | shasum -a 256
  sha256 "REPLACE_WITH_RELEASE_TARBALL_SHA256"
  license "GPL-3.0-only"
  head "https://github.com/Parzival1918/moltui.git", branch: "main"

  depends_on "cmake" => :build
  depends_on "ninja" => :build
  depends_on "cli11" => :build
  depends_on "ftxui" => :build
  depends_on "glm" => :build

  def install
    system "cmake", "-S", ".", "-B", "build", "-G", "Ninja",
           "-DMOLTUI_FETCH_DEPS=ON",
           "-DMOLTUI_BUILD_TESTS=OFF",
           *std_cmake_args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/moltui --version")
    output = shell_output("#{bin}/moltui -p -m ascii -W 40 -H 12")
    assert_predicate output, :present?
  end
end
