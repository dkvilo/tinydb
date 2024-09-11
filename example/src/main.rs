use actix_web::{web, App, HttpResponse, HttpServer, Responder};
use serde::{Deserialize, Serialize};
use std::sync::Mutex;

mod tinydb_client;
mod template;

#[derive(Deserialize, Serialize)]
struct TweetRequest {
    text: Option<String>,
    command: String,
}

struct AppState {
    db_client: Mutex<tinydb_client::TinyDBClient>,
}

async fn post_tweet(state: web::Data<AppState>, form: web::Form<TweetRequest>) -> impl Responder {
    let mut db_client = state.db_client.lock().unwrap();

    let command = form.command.clone();
    let response = match command.as_str() {
        "rpush" => match &form.text {
            Some(tweet) => match db_client.rpush("tweets", tweet) {
                Ok(_) => HttpResponse::Ok().body("Tweet posted successfully with RPUSH!"),
                Err(err) => HttpResponse::InternalServerError().body(format!("Error: {}", err)),
            },
            None => HttpResponse::BadRequest().body("Text is required for RPUSH!"),
        },
        "lpush" => match &form.text {
            Some(tweet) => match db_client.lpush("tweets", tweet) {
                Ok(_) => HttpResponse::Ok().body("Tweet posted successfully with LPUSH!"),
                Err(err) => HttpResponse::InternalServerError().body(format!("Error: {}", err)),
            },
            None => HttpResponse::BadRequest().body("Text is required for LPUSH!"),
        },
        "rpop" => match db_client.rpop("tweets") {
            Ok(tweet) => HttpResponse::Ok().body(format!("Tweet popped: {}", tweet)),
            Err(err) => HttpResponse::InternalServerError().body(format!("Error: {}", err)),
        },
        "lpop" => match db_client.lpop("tweets") {
            Ok(tweet) => HttpResponse::Ok().body(format!("Tweet popped: {}", tweet)),
            Err(err) => HttpResponse::InternalServerError().body(format!("Error: {}", err)),
        },
        _ => HttpResponse::BadRequest().body("Invalid command!"),
    };

    response
}

async fn fetch_tweets(state: web::Data<AppState> ) -> impl Responder {
    let mut db_client = state.db_client.lock().unwrap();
    
    let tweet_count = match db_client.llen("tweets") {
        Ok(count) => count.parse::<i32>().unwrap_or(0),
        Err(_) => 0,
    };

    let tweets_response = match db_client.get("tweets") {
        Ok(tweets) => tweets,
        Err(_) => "[]".to_string(),
    };

    let tweets_json = serde_json::from_str::<serde_json::Value>(&tweets_response).unwrap_or_else(|_| {
        serde_json::json!(["Unexpected format"])
    });

    HttpResponse::Ok().json(serde_json::json!({
        "tweets": tweets_json,
        "count": tweet_count
    }))
}


async fn tweet_page() -> impl Responder {
    HttpResponse::Ok().content_type("text/html").body(template::INDEX_HTML)
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    let client =  tinydb_client::TinyDBClient::new("127.0.0.1:8079").unwrap();
    let app_state = web::Data::new(AppState {
        db_client: Mutex::new(client),
    });

    HttpServer::new(move || {
        App::new()
            .app_data(app_state.clone())
            .route("/", web::get().to(tweet_page))
            .route("/tweets", web::post().to(post_tweet))
            .route("/tweets_list", web::post().to(fetch_tweets))
    })
    .bind("127.0.0.1:8080")?
    .run()
    .await
}
