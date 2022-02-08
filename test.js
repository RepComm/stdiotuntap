
let { spawn } = require("child_process");

let stdiotuntap = spawn("./stdiotuntap");

stdiotuntap.on("spawn", () => {
  // console.log("detected spawn of stdiotuntap");

  stdiotuntap.stdout.on('data', (data) => {
    console.log(`stdiotuntap stdout: ${data}`);

    /**@type {import("./src/protocol.d").JsonMsg} */
    let json = JSON.parse(data);
    


    switch(json.cmd) {
      case "log":
        /**@type {import("./src/protocol.d").JsonLogMsg}*/
        let logJson = json;

        console.log(logJson.log.data);
        break;
      case "data":
        /**@type {import("./src/protocol.d").JsonDataMsg}*/
        let dataJson = json;

        // dataJson.data.base64
        break;
      default:
        break;
    }
  });

  stdiotuntap.on('close', (code) => {
    console.log(`stdiotuntap exited with code ${code}`);
  });

  setInterval(() => {
    let msg = JSON.stringify(test());
    stdiotuntap.stdin.write(msg, (err) => {
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

/**
 * 
 * @returns {import("./src/protocol.d").JsonLogMsg} data
 */
function test () {
  return {
    cmd: "log",
    log: {
      data: "Hello world",
      error: "i hate this keyboard"
    }
  }
}

function main () {
  let json = test();
  let str = JSON.stringify(json);


}
