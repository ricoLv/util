fn main() {
    if std::env::var("TARGET")
        .unwrap_or_default()
        .contains("android")
    {
        cc::Build::new()
            .cpp(true)
            .shared_flag(true)
            .cpp_link_stdlib("c++_shared")
            .file("src/ifaces/ffi/android/ifaddrs.cpp")
            .compile("ifaddrs-android");
    }
}
