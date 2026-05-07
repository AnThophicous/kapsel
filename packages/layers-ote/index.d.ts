export interface ProxyEnv {
  profile: string;
  path: string;
}

export interface InjectOptions {
  cwd?: string;
  profile?: string;
  includeProfile?: boolean;
  binaryPath?: string;
}

export declare function readProxyEnv(cwd?: string): ProxyEnv;
export declare function resolveBinaryPath(options?: InjectOptions): string;
export declare function materialize(profile: string, cwd?: string, options?: InjectOptions): Record<string, string>;
export declare function injectIntoProcessEnv(options?: InjectOptions): Record<string, string>;
