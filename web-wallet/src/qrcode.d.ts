declare module "qrcode" {
  export function toCanvas(
    canvas: HTMLCanvasElement,
    text: string,
    opts?: { width?: number; margin?: number; color?: { dark?: string; light?: string } },
  ): Promise<void>;
}
