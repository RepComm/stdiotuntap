
export type CmdJson = "dev"|"data"|"die";
export type CmdDev = "up"|"down"|"create"|"destroy"|"sub"|"unsub";

export type IdDev = number;

export type Base64String = string;

export interface JsonMsg {
  /**when true, signals that this message is a response to a previous message with the same id*/
  isAck?: boolean;
  /**ID of message, populated by the sender, responded with by receiver*/
  id: number;

  /**Denotes interface extension to use
   * 
   * [key in CmdJson]?: {}
  */
  cmd: CmdJson;
}

export interface JsonDeviceMsg extends JsonMsg {
  cmd: "dev";
  dev: {
    cmd: CmdDev;
    id?: IdDev;
    ipv4?: string;
  }
}

export interface JsonDataMsg extends JsonMsg {
  cmd: "data";
  data: {
    base64?: Base64String;
  }
}


/**
 * Example:
 * Start a new device
 * `{"cmd":"dev","dev":{"cmd":"create"}}`
 * 
 * //attempting to create device
 * //tunnel created with fd 4 and ip 10.0.0.1
 * 
 * Set device up
 * `{"cmd":"dev","dev":{"cmd":"up","id":4}}`
 * 
 * Ping an endpoint using the device
 * `$ ping 10.1.1.0 -I tun0`
 * 
 * Watch the ping packets pour into the console, not being responded to.
 * 
 * Finally, to gracefully shutdown
 * `{"cmd":"die"}`
 */