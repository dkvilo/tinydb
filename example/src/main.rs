mod tinydb_client;

use actix_web::{web, App, HttpResponse, HttpServer, Responder};
use serde::{Deserialize, Serialize};
use serde_json;
use std::sync::Mutex;

#[derive(Deserialize, Serialize)]
struct Tweet {
    text: String,
}

struct AppState {
    db_client: Mutex<tinydb_client::TinyDBClient>,
}

async fn post_tweet(state: web::Data<AppState>, form: web::Form<Tweet>) -> impl Responder {
    let mut db_client = state.db_client.lock().unwrap();
    let tweet_content = form.text.clone();

    match db_client.rpush("tweets", &tweet_content) {
        Ok(_) => HttpResponse::Ok().body("Tweet posted successfully!"),
        Err(err) => HttpResponse::InternalServerError().body(format!("Error: {}", err)),
    }
}

async fn tweet_page() -> impl Responder {
    let html = r#"
        <h1>Tweet Something</h1>
        <form id="tweetForm">
            <input type="text" id="tweetText" name="text" placeholder="What's happening?" required>
            <button type="submit">Tweet</button>
        </form>

        <h2>Tweets</h2>
        <ul id="tweetList"></ul>

        <script>
        document.getElementById('tweetForm').addEventListener('submit', async function(event) {
            event.preventDefault(); // Prevent form from submitting normally
            let text = document.getElementById('tweetText').value;

            const response = await fetch('/tweets', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: new URLSearchParams({
                    'text': text
                })
            });

            if (response.ok) {
                document.getElementById('tweetText').value = '';
                updateTweetList(); // Refresh tweet list
            } else {
                alert('Failed to post the tweet.');
            }
        });

        async function updateTweetList() {
            const response = await fetch('/tweets_list');
            if (response.ok) {
                const tweets = await response.json();
                const tweetListElement = document.getElementById('tweetList');
                tweetListElement.innerHTML = '';

                tweets.forEach(tweet => {
                    const li = document.createElement('li');
                    li.textContent = tweet;
                    tweetListElement.appendChild(li);
                });
            }
        }

        // Initial fetch of tweet list
        updateTweetList();
        </script>
    "#;

    HttpResponse::Ok().content_type("text/html").body(html)
}

async fn fetch_tweets(state: web::Data<AppState>) -> impl Responder {
    let mut db_client = state.db_client.lock().unwrap();

    let tweets = match db_client.get("tweets") {
        Ok(tweets) => tweets,
        Err(_) => String::new(),
    };

    let tweets_list: Vec<String> = match serde_json::from_str(&tweets) {
        Ok(list) => list,
        Err(_) => vec![],
    };

    HttpResponse::Ok().json(tweets_list)
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    let client = tinydb_client::TinyDBClient::new("127.0.0.1:8079").unwrap();
    let app_state = web::Data::new(AppState {
        db_client: Mutex::new(client),
    });

    HttpServer::new(move || {
        App::new()
            .app_data(app_state.clone())
            .route("/", web::get().to(tweet_page))
            .route("/tweets", web::post().to(post_tweet))
            .route("/tweets_list", web::get().to(fetch_tweets))
    })
    .bind("127.0.0.1:8080")?
    .run()
    .await
}
