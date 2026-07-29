use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(
        env::var_os("CARGO_MANIFEST_DIR")
            .expect("CARGO_MANIFEST_DIR is not set"),
    );

    let lib_dir = env::var_os("DENSE_SIM_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| manifest_dir.join("../../lib/linux-x86_64"));

    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=static=dense_sim");
    println!("cargo:rustc-link-lib=m");
    println!("cargo:rerun-if-env-changed=DENSE_SIM_LIB_DIR");
    println!(
        "cargo:rerun-if-changed={}",
        manifest_dir
            .join("../../include/dense/dense_sim.h")
            .display()
    );
}
