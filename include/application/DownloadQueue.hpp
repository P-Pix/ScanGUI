/**
 * @file DownloadQueue.hpp
 * @brief Déclare la file de tâches asynchrones de l’application.
 *
 * La file permet d’exécuter en arrière-plan des opérations longues comme une synchronisation
 * offline, une mise à jour réseau ou une indexation OCR, sans bloquer l’interface GTK.
 */
#ifndef SCANGUI_APPLICATION_DOWNLOAD_QUEUE_HPP
#define SCANGUI_APPLICATION_DOWNLOAD_QUEUE_HPP

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

/**
 * @brief Contexte fourni à une tâche de téléchargement.
 *
 * La tâche peut tester `cancelled()` entre deux opérations longues et publier une progression.
 * L’annulation reste coopérative : la file ne tue jamais brutalement un thread en cours.
 */
struct DownloadJobContext {
    std::atomic<bool>& cancelRequested;
    std::function<bool()> pauseRequested;
    std::function<void()> waitWhilePaused;
    std::function<void(int current, int total, const std::string& message)> progress;

    [[nodiscard]] bool cancelled() const { return cancelRequested.load(); }
    [[nodiscard]] bool paused() const { return pauseRequested ? pauseRequested() : false; }
    void wait_if_paused() const {
        if (waitWhilePaused) waitWhilePaused();
    }
    void report(int current, int total, const std::string& message) const {
        if (progress) progress(current, total, message);
    }
};

/**
 * @brief Travail asynchrone soumis à la file de téléchargement.
 *
 * Le travail est représenté par un callback pour rester indépendant du type d’opération :
 * mise à jour de scan, synchronisation offline ou indexation OCR.
 */
struct DownloadJob {
    std::string id;
    std::function<void(DownloadJobContext&)> work;
    int maxRetries{2};
};

/**
 * @brief Résultat produit après l’exécution d’un travail de fond.
 *
 * Il permet à l’interface de distinguer un succès, une annulation volontaire ou une erreur
 * après retry sans inspecter les exceptions internes du worker.
 */
struct DownloadJobReport {
    std::string id;
    bool success{false};
    bool cancelled{false};
    int attempts{0};
    std::string error;
};

/**
 * @brief Petite file de travaux en arrière-plan pour synchronisations, mises à jour et OCR.
 *
 * La file ne connaît pas le contenu des travaux : elle exécute des callbacks. Cette séparation
 * permet de brancher la synchronisation offline, la mise à jour de scans ou une indexation OCR
 * sans coupler la GUI aux détails réseau ou disque.
 */
class DownloadQueue {
public:
    using CompletionCallback = std::function<void(const DownloadJobReport&)>;
    using ProgressCallback = std::function<void(const std::string& jobId, int current, int total, const std::string& message)>;

    explicit DownloadQueue(CompletionCallback onComplete = {}, ProgressCallback onProgress = {});
    ~DownloadQueue();

    /**
     * @brief Démarre le worker unique de la file.
     *
     * L’appel est idempotent : si la file tourne déjà, aucun second thread n’est créé.
     */
    void start();

    /**
     * @brief Arrête proprement le worker après réveil de la condition.
     *
     * Effet de bord : joint le thread de fond si celui-ci existe.
     */
    void stop();

    /**
     * @brief Demande l’annulation coopérative du travail courant et vide la file.
     *
     * Les jobs doivent consulter le contexte fourni pour s’arrêter sans interruption brutale.
     */
    void cancel();

    /** @brief Met en pause la consommation de nouveaux jobs. */
    void pause();

    /** @brief Reprend l’exécution et réveille le worker si nécessaire. */
    void resume();

    /** @brief Vide les travaux en attente sans interrompre directement le job courant. */
    void clear();

    /**
     * @brief Ajoute un travail en fin de file et réveille le worker.
     *
     * @param job Travail à exécuter en arrière-plan.
     */
    void push(DownloadJob job);

    [[nodiscard]] bool running() const;
    [[nodiscard]] bool paused() const;
    [[nodiscard]] bool cancellation_requested() const;
    [[nodiscard]] std::size_t pending_jobs() const;

private:
    CompletionCallback onComplete_;
    ProgressCallback onProgress_;
    std::queue<DownloadJob> jobs_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> cancelRequested_{false};
    std::atomic<bool> pauseRequested_{false};

    /**
     * @brief Boucle interne du worker.
     *
     * Elle attend les jobs, applique pause/reprise, exécute les callbacks avec retry et
     * publie un rapport final via le callback de complétion.
     */
    void loop();
};

#endif
