use crate::error::{invalid, Error, Result};
use mmx_wallet::KeyFile;
use std::{
    fs,
    io::Write,
    path::{Path, PathBuf},
};
use zeroize::Zeroizing;

pub fn directory() -> Result<PathBuf> {
    std::env::var_os("MMX_HOME")
        .filter(|s| !s.is_empty())
        .map(PathBuf::from)
        .or_else(|| {
            std::env::var_os("HOME")
                .filter(|s| !s.is_empty())
                .map(|p| PathBuf::from(p).join(".mmx"))
        })
        .map(|p| p.join("wallet"))
        .ok_or_else(|| {
            Error::new(
                "wallet_directory_unavailable",
                "set MMX_HOME or HOME, or select a wallet with --file",
            )
        })
}
pub fn absolute(path: &Path) -> Result<PathBuf> {
    Ok(if path.is_absolute() {
        path.to_owned()
    } else {
        std::env::current_dir()?.join(path)
    })
}
pub fn read_key(path: &Path) -> Result<KeyFile> {
    if fs::metadata(path)?.len() > 65536 {
        return Err(Error::new("wallet_error", "wallet file exceeds size limit"));
    }
    Ok(KeyFile::decode(&Zeroizing::new(fs::read(path)?))?)
}
fn parent(path: &Path) -> &Path {
    path.parent()
        .filter(|p| !p.as_os_str().is_empty())
        .unwrap_or(Path::new("."))
}
pub fn create_private_directory(path: &Path) -> Result<()> {
    let mut builder = fs::DirBuilder::new();
    builder.recursive(true);
    #[cfg(unix)]
    {
        use std::os::unix::fs::DirBuilderExt;
        builder.mode(0o700);
    }
    builder.create(path)?;
    Ok(())
}
pub fn save(path: &Path, bytes: &[u8], exists_code: Option<&'static str>) -> Result<()> {
    let parent = parent(path);
    let mut tmp = tempfile::NamedTempFile::new_in(parent)?;
    tmp.write_all(bytes)?;
    tmp.as_file().sync_all()?;
    if let Some(code) = exists_code {
        tmp.persist_noclobber(path).map_err(|e| {
            Error::new(
                if e.error.kind() == std::io::ErrorKind::AlreadyExists {
                    code
                } else {
                    "io_error"
                },
                "failed to save without overwriting; choose a new path",
            )
        })?;
    } else {
        tmp.persist(path).map_err(|e| Error::from(e.error))?;
    }
    #[cfg(unix)]
    fs::File::open(parent)?.sync_all()?;
    Ok(())
}
pub fn save_key(path: &Path, key: &KeyFile) -> Result<()> {
    if fs::symlink_metadata(path).is_ok() {
        return Err(Error::new(
            "wallet_exists",
            "wallet already exists; choose a new path",
        ));
    }
    create_private_directory(parent(path))?;
    let bytes = Zeroizing::new(if path.extension().is_some_and(|e| e == "json") {
        serde_json::to_vec(key).expect("key serialization")
    } else {
        key.encode()
    });
    save(path, &bytes, Some("wallet_exists"))
}
#[derive(Clone)]
pub struct Entry {
    pub path: PathBuf,
    pub fingerprint: String,
    pub with_passphrase: bool,
}
pub fn wallets(dir: &Path) -> Result<Vec<Entry>> {
    if !dir.exists() {
        return Ok(Vec::new());
    }
    let mut entries = Vec::new();
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        if !entry.file_type()?.is_file() {
            continue;
        }
        let name = entry.file_name();
        let name = name.to_string_lossy();
        if name != "wallet.dat"
            && !(name.ends_with(".dat")
                && (name.starts_with("wallet_") || name.starts_with("mmxwallet_")))
        {
            continue;
        }
        let key = read_key(&entry.path())?;
        entries.push(Entry {
            path: entry.path(),
            fingerprint: key.fingerprint(),
            with_passphrase: key.requires_passphrase(),
        });
    }
    entries.sort_by(|a, b| a.path.file_name().cmp(&b.path.file_name()));
    Ok(entries)
}
pub fn active(dir: &Path) -> Result<Option<String>> {
    let path = dir.join("mmxwallet.json");
    if !path.exists() {
        return Ok(None);
    }
    let v: serde_json::Value = serde_json::from_slice(&fs::read(path)?)
        .map_err(|_| invalid("invalid wallet configuration"))?;
    Ok(v["active_wallet"]
        .as_str()
        .filter(|s| !s.is_empty())
        .map(str::to_owned))
}
pub fn set_active(dir: &Path, path: &Path) -> Result<()> {
    create_private_directory(dir)?;
    let config = dir.join("mmxwallet.json");
    let mut v = if config.exists() {
        serde_json::from_slice(&fs::read(&config)?)
            .map_err(|_| invalid("invalid wallet configuration"))?
    } else {
        serde_json::json!({})
    };
    if !v.is_object() {
        return Err(invalid("invalid wallet configuration"));
    }
    v["active_wallet"] = serde_json::json!(path
        .file_name()
        .ok_or_else(|| invalid("invalid wallet filename"))?
        .to_string_lossy());
    save(
        &config,
        &serde_json::to_vec(&v).expect("config serialization"),
        None,
    )
}
pub fn select(
    entries: &[Entry],
    selector: Option<&str>,
    active: Option<&str>,
    allow_index: bool,
) -> Result<Entry> {
    if entries.is_empty() {
        return Err(Error::new(
            "wallet_error",
            "no wallets found; run 'mmxwallet create' or 'mmxwallet import'",
        ));
    }
    if let Some(s) = selector {
        if !s.starts_with('#') {
            let matches: Vec<_> = entries.iter().filter(|e| e.fingerprint == s).collect();
            if matches.len() > 1 {
                return Err(invalid("wallet fingerprint is ambiguous; select by index"));
            }
            if let Some(e) = matches.first() {
                return Ok((*e).clone());
            }
        }
        if allow_index {
            if let Ok(i) = s.trim_start_matches('#').parse::<usize>() {
                if let Some(e) = entries.get(i) {
                    return Ok(e.clone());
                }
            }
        }
        return Err(invalid("no wallet matches selector"));
    }
    if let Some(a) = active {
        if let Some(e) = entries
            .iter()
            .find(|e| e.path.file_name().is_some_and(|n| n == a))
        {
            return Ok(e.clone());
        }
        return select(entries, Some(a), None, false);
    }
    if entries.len() == 1 {
        return Ok(entries[0].clone());
    }
    Err(Error::new(
        "wallet_error",
        "multiple wallets found; select with 'mmxwallet use' or --wallet",
    ))
}
