Pod::Spec.new do |s|
  s.name         = 'AnyChatSDK'
  s.version      = '0.1.0'
  s.summary      = 'Complete Swift SDK for the AnyChat IM system with messaging, voice/video calls, and group management.'
  s.description  = <<-DESC
    AnyChatSDK provides a modern Swift API for the AnyChat instant messaging system.
    Features include:
    - Modern async/await Swift API
    - AsyncStream-based event handling
    - Full type safety with Swift structs
    - Thread-safe actor-based design
    - Voice and video calling support
    - Group chat and friend management
    - File upload/download with progress
    - Support for iOS 13+ and macOS 10.15+
  DESC

  s.homepage     = 'https://github.com/yzhgit/anychat-sdk'
  s.license      = { :type => 'MIT', :file => 'LICENSE' }
  s.author       = { 'AnyChat Team' => 'sdk@anychat.io' }

  s.ios.deployment_target = '13.0'
  s.osx.deployment_target = '10.15'

  s.source       = { :git => 'https://github.com/yzhgit/anychat-sdk.git', :tag => "v#{s.version}" }

  s.swift_version = '5.9'

  # Swift source files
  s.source_files = [
    'Sources/AnyChatSDK/**/*.{swift,h}',
    'core/include/**/*.h'
  ]

  # Public headers
  s.public_header_files = [
    'Sources/AnyChatSDK/AnyChatSDK.h',
    'core/include/**/*.h'
  ]

  # Module map
  s.module_map = 'Sources/AnyChatSDK/module.modulemap'

  # Preserve directory structure for headers
  s.header_mappings_dir = '.'

  # Vendored library (built by prepare_command)
  s.vendored_libraries = 'build/libAnyChatCore.a'

  # System frameworks
  s.ios.frameworks = ['Foundation', 'Security', 'SystemConfiguration']
  s.osx.frameworks = ['Foundation', 'Security', 'SystemConfiguration']

  # C++ standard library (required by core library)
  s.libraries = 'c++'

  # Build settings
  s.pod_target_xcconfig = {
    'SWIFT_VERSION' => '5.9',
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++23',
    'CLANG_CXX_LIBRARY' => 'libc++',
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) ANYCHAT_COCOAPODS=1',
    'HEADER_SEARCH_PATHS' => '$(inherited) "${PODS_ROOT}/AnyChatSDK/core/include" "${PODS_ROOT}/AnyChatSDK/thirdparty/curl/include" "${PODS_ROOT}/AnyChatSDK/thirdparty/libwebsockets/include" "${PODS_ROOT}/AnyChatSDK/thirdparty/glaze/include" "${PODS_ROOT}/AnyChatSDK/thirdparty/sqlite3"',
    'OTHER_LDFLAGS' => '$(inherited) -framework Foundation -framework Security -framework SystemConfiguration'
  }

  # Build the C++ core library before pod installation
  s.prepare_command = <<-CMD
    # Initialize submodules if not already done
    if [ ! -f "thirdparty/curl/configure" ]; then
      git submodule update --init --recursive
    fi

    # Create build directory
    mkdir -p build

    # Build static library for iOS
    cd build
    cmake ../core \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
      -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_TESTING=OFF

    cmake --build . --target anychat_core --config Release

    # Copy the built library
    cp core/libanychat_core.a libAnyChatCore.a || cp core/Release/libanychat_core.a libAnyChatCore.a || true
  CMD

  # Documentation
  s.documentation_url = 'https://yzhgit.github.io/anychat-server'

  # Social media
  s.social_media_url = 'https://github.com/yzhgit'

  # Additional metadata
  s.requires_arc = true
  s.static_framework = true
end
