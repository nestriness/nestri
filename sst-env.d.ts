/* tslint:disable */
/* eslint-disable */
import "sst"
declare module "sst" {
  export interface Resource {
    "CloudflareAccountId": {
      "type": "sst.sst.Secret"
      "value": string
    }
    "Relay": {
      "type": "sst.aws.Service"
      "url": string
    }
  }
}
export {}
