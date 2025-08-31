use rand::{Rng, SeedableRng};
use rand::rngs::StdRng;
use std::{sync::Arc, time::Instant};
use tokio::{sync::Semaphore, time::{sleep, Duration}};
use futures::stream::{self, StreamExt};

async fn simulate_api_call(id: usize) -> bool { 
    let delay_ms: u64 = 100;
    sleep(Duration::from_millis(delay_ms)).await;

    println!("RUN - Task[{}] run ended after {}ms.", id, delay_ms);

    let is_successful: bool = StdRng::from_entropy().gen_bool(0.8);
    is_successful
}

async fn call_api(id: usize) {
    let mut tries: i32 = 0;
    
    loop {
        tries += 1;
        
        if simulate_api_call(id).await {
            println!("SUCCESS - Task[{}] finished successfully after {} tries.", id, tries);
            break;
        } else {
            println!("FAIL - Task[{}] failed.", id);
        }
    }
}

async fn run_with_buffer_unordered(task_count: usize, max_connections: usize) -> std::time::Duration {
    let start: Instant = Instant::now();

    stream::iter(0..task_count)
        .map(|id: usize| call_api(id))
        .buffer_unordered(max_connections)
        .collect::<Vec<_>>()
        .await;

    start.elapsed()
}

async fn run_with_semaphore(task_count: usize, max_connections: usize) -> std::time::Duration {
    let start: Instant = Instant::now();

    let semaphore: Arc<Semaphore> = Arc::new(Semaphore::new(max_connections));
    let mut handles: Vec<tokio::task::JoinHandle<()>> = vec![];

    for id in 0..task_count {
        let permit: tokio::sync::OwnedSemaphorePermit = semaphore.clone().acquire_owned().await.unwrap();

        let handle: tokio::task::JoinHandle<()> = tokio::spawn(async move {
            call_api(id).await;
            
            drop(permit);
        });

        handles.push(handle);
    }
    futures::future::join_all(handles).await;

    start.elapsed()
}

#[tokio::main]
async fn main() {
    let task_count = 1000;
    let max_connections = 5;

    println!("START - Buffered Unordered started...");
    let time_buffer_unordered = run_with_buffer_unordered(task_count, max_connections).await;

    println!("START - Semaphore Unordered started...");
    let time_semaphore = run_with_semaphore(task_count, max_connections).await;

    println!("END - Total time - Buffered Unordered: {:?}", time_buffer_unordered);
    println!("END - Total time - Semaphore: {:?}", time_semaphore);
}
