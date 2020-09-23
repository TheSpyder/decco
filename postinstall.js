const path = require("path");
const fs = require("fs");

const installMacLinuxBinary = (binary) => {
  const source = path.join(__dirname, binary);
  if (fs.existsSync(source)) {
    const target = path.join(__dirname, "ppx")
    fs.renameSync(source, target)
    // it should be executable in the bundle, but just in case
    fs.chmodSync(target, 0777)
  } else {
    // assume we're in dev mode - nothing will break if the script
    // isn't overwritten, it will just be slower
  }
}

const installWindowsBinary = () => {
  const source = path.join(__dirname, "ppx-windows.exe")
  if (fs.existsSync(source)) {
    const windowsScript = path.join(__dirname, "ppx.cmd")
    const target = path.join(__dirname, "ppx.exe")
    fs.unlinkSync(windowsScript)
    fs.renameSync(source, target)
  } else {
    // assume we're in dev mode - nothing will break if the script
    // isn't overwritten, it will just be slower
  }
}



switch (process.platform) {
  case "linux":
    installMacLinuxBinary("ppx-linux.exe")
    break;
  case "darwin":
    installMacLinuxBinary("ppx-macos.exe")
    break;
  case "win32":
    installWindowsBinary()
  default:
    // This won't break the installation because the `ppx` shell script remains
    // but that script will throw an error in this case anyway
    console.warn(`No release available for "${process.platform}"`)
    process.exit(1)
}
