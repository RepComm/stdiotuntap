
let { spawn } = require("child_process");

let stdiotuntap = spawn("./stdiotuntap");

stdiotuntap.on("spawn", () => {
  // console.log("detected spawn of stdiotuntap");

  stdiotuntap.stdout.on('data', (data) => {
    console.log(`stdiotuntap stdout: ${data}`);
  });

  stdiotuntap.stderr.on('data', (data) => {
    console.error(`stdiotuntap stderr: ${data}`);
  });

  stdiotuntap.on('close', (code) => {
    console.log(`stdiotuntap exited with code ${code}`);
  });

  setInterval(() => {
    stdiotuntap.stdin.write("{hi}", (err) => {
      if (err) console.log("failed to write", err);
    });
  }, 500);
});


setTimeout(() => {
  console.log("finished");
  process.exit(0);
}, 10000);

function ipToHex(ip) {
  let bytes = [];
  let parts = ip.split(".");
  for (let i = 0; i < 4; i++) {
    bytes[i] = parseInt(parts[i]);
  }
  let result = "";
  for (let i = 0; i < 4; i++) {
    let ch = bytes[i].toString(16);
    if (ch.length == 1) ch = "0" + ch;
    result += ch;
  }
  return result;
}

