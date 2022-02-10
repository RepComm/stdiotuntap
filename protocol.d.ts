
/**
 * @version 0.5
 */

 export type CmdJson = "dev"|"data"|"die"|"log";
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
    srcIpv4?: string;
    destIpv4?: string;
    base64?: Base64String;
   }
 }
 
 export interface JsonLogMsg extends JsonMsg {
   cmd: "log",
   log: {
     data?: string;
     error?: string;
   }
 }
 
 /**
  * Example:
  * 
  * Send to create a device:
  * `{"cmd":"dev","dev":{"cmd":"create"}}`
  * Response:
  * {"cmd":"dev","dev":{"cmd":"create","id":4,"ifname":"tun0","ipv4":"10.0.0.1"}}
  * 
  * Send to 'up' device 4:
  * `{"cmd":"dev","dev":{"cmd":"up","id":4}}`
  * Response:
  * {"cmd":"dev","dev":{"cmd":"up","id":4}}
  * 
  * Ping an endpoint using the device
  * `$ ping 10.1.1.0 -I tun0`
  * 
  * Watch the ping packets pour into the console, not being responded to.
  * 
  * Finally, to gracefully shutdown
  * `{"cmd":"die"}`
  */