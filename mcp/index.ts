import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";
import { exec } from "node:child_process";
import { tmpdir } from "node:os";
import path from "node:path";
import fs from "node:fs/promises";
import { randomUUID } from "node:crypto";

const CLOX_EXECUTABLE = process.env.CLOX_EXECUTABLE;
if (!CLOX_EXECUTABLE) {
  throw new Error("CLOX_EXECUTABLE environment variable is not set.");
}

const server = new McpServer({
  name: "clox-mcp",
  version: "1.0.0",
});

server.tool(
  "execute_clox_code",
  "A tool that executes clox code and returns standard output.",
  {
    code: z.string().describe("The clox code to execute."),
  },
  async ({ code }) => {
    const filename = path.join(tmpdir(), `code_${randomUUID()}.clox`);

    try {
      await fs.writeFile(filename, code, "utf-8");
      const stdout = await new Promise<string>((resolve, reject) => {
        exec(`${CLOX_EXECUTABLE} ${filename}`, (error, stdout) => {
          if (error) {
            reject(error);
          } else {
            resolve(stdout);
          }
        });
      });

      return {
        content: [
          {
            type: "text",
            text: stdout,
          },
        ],
      };
    } catch (err) {
      return {
        content: [
          {
            type: "text",
            text: `Error: ${err}`,
          },
        ],
        isError: true,
      };
    } finally {
      try {
        await fs.unlink(filename);
      } catch {
        // ignore
      }
    }
  }
);

const transport = new StdioServerTransport();
await server.connect(transport);
console.error("Clox MCP Server is running...");
