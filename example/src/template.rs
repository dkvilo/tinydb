pub const INDEX_HTML: &str = r#"

<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<style>
  body {
    font-family: Arial, sans-serif;
    display: flex;
    flex-direction: column;
    justify-content: center;
    max-width: 70%;
    align-self: center;
    margin: auto;
  }
  
  h1, h2 {
    color: #333;
  }

  form {
    margin-bottom: 20px;
  }

  textarea
  {
    width: 100%;
    height: 150px;
    margin-top: 20px;
  }

  textarea,
  input {
    padding: 8px;
    font-size: 16px;
  }

  button {
    padding: 8px 16px;
    font-size: 16px;
    margin-right: 5px;
    cursor: pointer;
  }

  .group {
    margin-top: 20px;
  }
  
  ul {
    list-style-type: none;
    padding-left: 0;
  }

  li {
    padding: 10px;
    background-color: #f4f4f4;
    margin-bottom: 5px;
    border-radius: 4px;
  }
</style>
</head>

<body>
<form id="tweetForm">
  <textarea
    type="text"
    id="tweetText"
    name="text"
    placeholder="What's happening?"
    required
  ></textarea>
  <div class="group">
    <button type="submit" name="command" value="rpush">RPUSH (Bottom)</button>
    <button type="submit" name="command" value="lpush">LPUSH (Top)</button>
  </div>
</form>

<div class="group">
  <button id="rpopButton">RPOP (Bottom)</button>
  <button id="lpopButton">LPOP (Top)</button>
</div>

<h2>Tweets (<span id="tweetCount"></span>)</h2>
<ul id="tweetList"></ul>

<script>
  const eventSource = new EventSource("/events");
  eventSource.onmessage = function(event) {
    if (event.data === "new_tweet") {
      updateTweetList();
    }
  };

  document.getElementById("tweetForm").addEventListener("submit", async function (event) {
    event.preventDefault();

    let text = document.getElementById("tweetText").value;
    let command = event.submitter.value;

    let bodyParams = new URLSearchParams();
    bodyParams.append("command", command);
    bodyParams.append("text", text);

    const response = await fetch("/tweets", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: bodyParams.toString(),
    });

    if (response.ok) {
      document.getElementById("tweetText").value = "";
    } else {
      alert("Failed to process the command.");
    }
  });

  async function updateTweetList() {
    const bodyParams = new URLSearchParams();
    bodyParams.append("command", "lrange");

    const response = await fetch("/tweets_list", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: bodyParams.toString(),
    });

    if (response.ok) {
      const data = await response.json();
      const tweets = data.tweets || [];

      document.getElementById("tweetCount").textContent = data.count;
      const tweetListElement = document.getElementById("tweetList");
      tweetListElement.innerHTML = "";

      if (Array.isArray(tweets)) {
        tweets.forEach((tweet) => {
          const li = document.createElement("li");
          li.textContent = tweet;
          tweetListElement.appendChild(li);
        });
      }
    }
  }

  document.getElementById("rpopButton").addEventListener("click", async () => {
    const bodyParams = new URLSearchParams();
    bodyParams.append("command", "rpop");

    const response = await fetch("/tweets", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: bodyParams.toString(),
    });

    if (response.ok) {
      updateTweetList();
    } else {
      alert("Failed to process RPOP.");
    }
  });

  document.getElementById("lpopButton").addEventListener("click", async () => {
    const bodyParams = new URLSearchParams();
    bodyParams.append("command", "lpop");

    const response = await fetch("/tweets", {
      method: "POST",
      headers: {
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: bodyParams.toString(),
    });

    if (response.ok) {
      updateTweetList();
    } else {
      alert("Failed to process LPOP.");
    }
  });

  updateTweetList();
</script>
</body>
</html>
"#;
