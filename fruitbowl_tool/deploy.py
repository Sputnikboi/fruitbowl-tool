"""
Deploy logic for the Fruitbowl Resource Pack Tool.
Zip the pack, compute SHA1, upload to GitHub Releases, and update
server.properties accordingly.
"""

import hashlib
import json
import os
import re
import shutil
import subprocess
import zipfile

# GitHub release config
GH_REPO = "Sputnikboi/fruitbowl-tool"
GH_RELEASE_TAG = "pack-v1"
GH_ASSET_NAME = "fruitbowl-server-pack.zip"


def zip_pack(pack_root: str, output_path: str | None = None) -> str:
    """
    Zip a resource pack directory into a .zip file.
    Excludes non-pack files (model list.txt, .git, etc.).
    Returns the path to the created zip.
    """
    if output_path is None:
        output_path = os.path.join(
            os.path.dirname(pack_root), "fruitbowl-server-pack.zip")

    # Remove old zip if it exists
    if os.path.exists(output_path):
        os.remove(output_path)

    exclude = {
        "model list.txt",
        "how to add to pack.txt",
        "fruitbowl elytra commands.txt",
        ".git",
        ".gitignore",
        "__pycache__",
    }

    with zipfile.ZipFile(output_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for dirpath, dirnames, filenames in os.walk(pack_root):
            # Skip excluded directories
            dirnames[:] = [d for d in dirnames if d not in exclude]

            for filename in filenames:
                if filename in exclude:
                    continue
                # Skip zip files inside the pack
                if filename.endswith(".zip"):
                    continue

                full_path = os.path.join(dirpath, filename)
                arc_name = os.path.relpath(full_path, pack_root)
                zf.write(full_path, arc_name)

    return output_path


def compute_sha1(file_path: str) -> str:
    """Compute SHA1 hash of a file."""
    sha1 = hashlib.sha1()
    with open(file_path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            sha1.update(chunk)
    return sha1.hexdigest()


def get_pack_url() -> str:
    """Get the GitHub Releases download URL for the pack."""
    return f"https://github.com/{GH_REPO}/releases/download/{GH_RELEASE_TAG}/{GH_ASSET_NAME}"


def upload_to_github(zip_path: str) -> list[tuple[str, str]]:
    """
    Upload the pack zip to GitHub Releases, replacing any existing asset.
    Requires `gh` CLI to be installed and authenticated.
    Returns a list of (tag, message) log tuples.
    """
    log = []

    # Check gh is available
    try:
        result = subprocess.run(
            ["gh", "--version"], capture_output=True, text=True, timeout=5)
        if result.returncode != 0:
            return [("error", "gh CLI not found. Install with: sudo apt install gh")]
    except FileNotFoundError:
        return [("error", "gh CLI not found. Install with: sudo apt install gh")]

    # Delete existing asset if present (gh will error on duplicate names)
    subprocess.run(
        ["gh", "release", "delete-asset", GH_RELEASE_TAG, GH_ASSET_NAME,
         "--repo", GH_REPO, "--yes"],
        capture_output=True, text=True, timeout=30)
    log.append(("info", "  Removed old asset (if any)"))

    # Upload new asset
    result = subprocess.run(
        ["gh", "release", "upload", GH_RELEASE_TAG, zip_path,
         "--repo", GH_REPO, "--clobber"],
        capture_output=True, text=True, timeout=120)

    if result.returncode == 0:
        url = get_pack_url()
        log.append(("success", f"✓ Uploaded to GitHub Releases"))
        log.append(("success", f"✓ URL: {url}"))
    else:
        log.append(("error", f"Upload failed: {result.stderr.strip()}"))

    return log


def update_server_properties(
    server_dir: str,
    pack_url: str,
    sha1: str,
    require: bool = True,
    prompt: str = "",
) -> list[tuple[str, str]]:
    """
    Update server.properties with the resource pack URL and SHA1.
    Returns a list of (tag, message) log tuples.
    """
    log = []
    props_path = os.path.join(server_dir, "server.properties")

    if not os.path.exists(props_path):
        return [("error", f"server.properties not found at {props_path}")]

    with open(props_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    replacements = {
        "resource-pack=": f"resource-pack={pack_url}\n",
        "resource-pack-sha1=": f"resource-pack-sha1={sha1}\n",
        "require-resource-pack=": f"require-resource-pack={'true' if require else 'false'}\n",
        "resource-pack-prompt=": f"resource-pack-prompt={prompt}\n",
    }

    new_lines = []
    matched_keys = set()
    for line in lines:
        replaced = False
        for key, replacement in replacements.items():
            if line.startswith(key):
                new_lines.append(replacement)
                matched_keys.add(key)
                replaced = True
                break
        if not replaced:
            new_lines.append(line)

    # Add any missing keys
    for key, replacement in replacements.items():
        if key not in matched_keys:
            new_lines.append(replacement)

    with open(props_path, "w", encoding="utf-8") as f:
        f.writelines(new_lines)

    log.append(("success", f"✓ resource-pack URL set"))
    log.append(("success", f"✓ resource-pack-sha1 = {sha1}"))
    log.append(("success", f"✓ require-resource-pack = {require}"))

    return log


def deploy_full(
    pack_root: str,
    server_dir: str,
) -> list[tuple[str, str]]:
    """
    Full deploy: zip -> upload to GitHub -> update server.properties.
    Returns a list of (tag, message) log tuples.
    """
    from fruitbowl_tool.core import sync_helmets

    log = []

    # 0 - Sync helmet dispatches from stone_button
    helmet_log = sync_helmets(pack_root)
    if helmet_log:
        log.append(("info", "Syncing helmet dispatches..."))
        log.extend(helmet_log)

    # 1 - Zip
    zip_path = zip_pack(pack_root)
    zip_size = os.path.getsize(zip_path)
    log.append(("success", f"✓ Packed ({zip_size // 1024} KB)"))

    # 2 - SHA1
    sha1 = compute_sha1(zip_path)
    log.append(("success", f"✓ SHA1: {sha1}"))

    # 3 - Upload to GitHub
    upload_log = upload_to_github(zip_path)
    log.extend(upload_log)

    # Check if upload succeeded
    if any(tag == "error" for tag, _ in upload_log):
        return log

    # 4 - Update server.properties
    url = get_pack_url()
    props_log = update_server_properties(server_dir, url, sha1)
    log.extend(props_log)

    log.append(("info", ""))
    log.append(("info", "  Restart the server for changes to take effect."))

    return log
