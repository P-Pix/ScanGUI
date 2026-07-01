/**
 * @file DownloadQueue.cpp
 * @brief Implémente la file de travaux asynchrones utilisée par les synchronisations longues.
 *
 * Le worker exécute des callbacks métier hors du thread GTK. Les traitements restent coopératifs :
 * ils consultent le contexte pour respecter pause, reprise et annulation.
 */
#include "application/DownloadQueue.hpp"

#include <stdexcept>
#include <utility>

DownloadQueue::DownloadQueue(CompletionCallback onComplete, ProgressCallback onProgress)
    : onComplete_(std::move(onComplete)), onProgress_(std::move(onProgress)) {}

DownloadQueue::~DownloadQueue() { stop(); }

/**
 * @brief Lance le worker si la file n'est pas déjà active.
 */
void DownloadQueue::start() {
    if (running_.exchange(true)) return;
    stopRequested_ = false;
    cancelRequested_ = false;
    pauseRequested_ = false;
    worker_ = std::thread([this] { loop(); });
}

/**
 * @brief Arrête proprement le worker et attend la fin du thread.
 */
void DownloadQueue::stop() {
    stopRequested_ = true;
    pauseRequested_ = false;
    condition_.notify_all();
    if (worker_.joinable()) worker_.join();
    running_ = false;
}

/**
 * @brief Demande l'annulation coopérative et vide les travaux en attente.
 */
void DownloadQueue::cancel() {
    cancelRequested_ = true;
    pauseRequested_ = false;
    clear();
    condition_.notify_all();
}

void DownloadQueue::pause() { pauseRequested_ = true; }

void DownloadQueue::resume() {
    pauseRequested_ = false;
    condition_.notify_all();
}

void DownloadQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<DownloadJob> empty;
    std::swap(jobs_, empty);
}

/**
 * @brief Ajoute un travail à la file et réveille le worker.
 *
 * @param job Travail applicatif contenant identifiant, callback et nombre de retries.
 */
void DownloadQueue::push(DownloadJob job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push(std::move(job));
    }
    cancelRequested_ = false;
    condition_.notify_one();
}

bool DownloadQueue::running() const { return running_; }
bool DownloadQueue::paused() const { return pauseRequested_; }
bool DownloadQueue::cancellation_requested() const { return cancelRequested_; }

std::size_t DownloadQueue::pending_jobs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_.size();
}

/**
 * @brief Boucle du worker exécutant les travaux avec retry, pause et rapports.
 *
 * Les exceptions des callbacks sont capturées et converties en `DownloadJobReport` pour éviter
 * qu'une tâche réseau fasse tomber l'application GTK.
 */
void DownloadQueue::loop() {
    while (!stopRequested_) {
        DownloadJob job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] {
                return stopRequested_ || (!pauseRequested_ && !jobs_.empty());
            });
            if (stopRequested_) break;
            if (jobs_.empty()) continue;
            job = std::move(jobs_.front());
            jobs_.pop();
        }

        DownloadJobReport report;
        report.id = job.id;
        for (int attempt = 1; attempt <= job.maxRetries + 1; ++attempt) {
            report.attempts = attempt;
            report.cancelled = false;
            try {
                if (!job.work) throw std::runtime_error("empty job");
                DownloadJobContext context{
                    cancelRequested_,
                    [this]() { return pauseRequested_.load(); },
                    [this]() {
                        std::unique_lock<std::mutex> lock(mutex_);
                        condition_.wait(lock, [this] {
                            return stopRequested_.load() || cancelRequested_.load() || !pauseRequested_.load();
                        });
                    },
                    [this, jobId = job.id](int current, int total, const std::string& message) {
                        if (onProgress_) onProgress_(jobId, current, total, message);
                    }
                };
                if (context.cancelled()) {
                    report.cancelled = true;
                    report.error = "cancelled before start";
                    break;
                }
                context.report(0, 0, "demarrage");
                job.work(context);
                if (context.cancelled()) {
                    report.cancelled = true;
                    report.error = "cancelled";
                    break;
                }
                report.success = true;
                report.error.clear();
                break;
            } catch (const std::exception& error) {
                report.error = error.what();
                if (cancelRequested_) {
                    report.cancelled = true;
                    break;
                }
            }
        }
        if (onComplete_) onComplete_(report);
    }
}
