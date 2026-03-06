"""
Deploy logic for the Fruitbowl Resource Pack Tool.
Zip the pack, compute SHA1, upload to mc-packs.net (manual) or
self-host via the Minecraft server's BlueMap webroot, and update
server.properties accordingly.
"""

import hashlib
import json
import os
import re
import shutil
import subprocess
import zipfile


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


def update_server_properties(
    server_dir: str,
    pack_url: str,
    sha1: str,
    require: bool = True,
    prompt: str = "Please enable the Fruitbowl resource pack!",
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

    # Escape colons in URL for Java properties format
    escaped_url = pack_url.replace(":", "\\:")

    replacements = {
        "resource-pack=": f"resource-pack={escaped_url}\n",
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


def deploy_to_bluemap(
    pack_root: str,
    server_dir: str,
    bluemap_port: int | None = None,
) -> list[tuple[str, str]]:
    """
    Zip the pack, copy to BlueMap webroot, and update server.properties
    to serve it from the BlueMap web server.
    Returns a list of (tag, message) log tuples.
    """
    log = []

    # 1 — Zip the pack
    zip_path = zip_pack(pack_root)
    zip_size = os.path.getsize(zip_path)
    log.append(("success", f"✓ Packed → {os.path.basename(zip_path)} ({zip_size // 1024}KB)"))

    # 2 — SHA1
    sha1 = compute_sha1(zip_path)
    log.append(("success", f"✓ SHA1: {sha1}"))

    # 3 — Find BlueMap webroot
    webroot = os.path.join(server_dir, "bluemap", "web")
    if not os.path.isdir(webroot):
        log.append(("warn", "⚠ BlueMap webroot not found — zip created but not deployed"))
        log.append(("info", f"  Zip is at: {zip_path}"))
        log.append(("info", "  Upload manually to mc-packs.net"))
        return log

    # Copy zip to webroot
    dest = os.path.join(webroot, "fruitbowl-pack.zip")
    shutil.copy2(zip_path, dest)
    log.append(("success", f"✓ Copied to BlueMap webroot"))

    # 4 — Determine the port from BlueMap config
    if bluemap_port is None:
        bluemap_port = _read_bluemap_port(server_dir)

    if bluemap_port:
        # The URL players connect to — this needs to be accessible from outside
        # For now, use localhost; the user will need to set up a tunnel
        log.append(("info",
                     f"  BlueMap web server on port {bluemap_port}"))
        log.append(("info",
                     "  Set resource-pack URL to your public BlueMap address + /fruitbowl-pack.zip"))
    else:
        log.append(("warn", "⚠ Could not determine BlueMap port"))

    return log


def deploy_to_mcpacks(
    pack_root: str,
    server_dir: str,
) -> list[tuple[str, str]]:
    """
    Zip the pack, compute SHA1, and prepare for mc-packs.net upload.
    Returns a list of (tag, message) log tuples.
    """
    log = []

    # 1 — Zip
    zip_path = zip_pack(pack_root)
    zip_size = os.path.getsize(zip_path)
    log.append(("success", f"✓ Packed → {os.path.basename(zip_path)} ({zip_size // 1024}KB)"))

    # 2 — SHA1
    sha1 = compute_sha1(zip_path)
    log.append(("success", f"✓ SHA1: {sha1}"))

    # 3 — Expected URL after upload
    expected_url = f"https://download.mc-packs.net/pack/{sha1}.zip"
    log.append(("info", f"  Expected URL: {expected_url}"))
    log.append(("info", f"  Zip location: {zip_path}"))
    log.append(("info", ""))
    log.append(("info", "  Upload the zip to https://mc-packs.net/"))
    log.append(("info", "  Then click 'Update server.properties' to apply."))

    return log, zip_path, sha1


def _read_bluemap_port(server_dir: str) -> int | None:
    """Read port from BlueMap webserver.conf."""
    conf_path = os.path.join(server_dir, "config", "bluemap", "webserver.conf")
    if not os.path.exists(conf_path):
        return None
    try:
        with open(conf_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line.startswith("port:"):
                    return int(line.split(":")[1].strip())
    except Exception:
        pass
    return None
