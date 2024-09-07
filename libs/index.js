const TinyDBClient = require("./libtinydb");

async function main() {
  const client = new TinyDBClient("127.0.0.1", 8079);

  try {
    await client.connect();
    console.log("Connected to TinyDB");

    await client.set("mykey", "Hello, TinyDB!");
    console.log("Set key: mykey");

    const value = await client.get("2000");
    console.log("Got value:", value);

    await client.append("mykey", " Appended text.");
    console.log("Appended to mykey");

    const length = await client.strlen("mykey");
    console.log("String length:", length);
  } catch (error) {
    console.error("Error:", error);
  } finally {
    await client.disconnect();
    console.log("Disconnected from TinyDB");
  }
}

main();
