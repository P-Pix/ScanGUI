CREATE TABLE IF NOT EXISTS scans (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    folder_name TEXT NOT NULL,
    root_path TEXT NOT NULL,
    download_url TEXT NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS chapters (
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
    path TEXT NOT NULL,
    PRIMARY KEY (scan_id, chapter_number)
);

CREATE TABLE IF NOT EXISTS pages (
    scan_id TEXT NOT NULL,
    chapter_number INTEGER NOT NULL,
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    file_path TEXT NOT NULL,
    mime_type TEXT NOT NULL,
    size_bytes BIGINT NOT NULL DEFAULT 0,
    PRIMARY KEY (scan_id, chapter_number, page_number),
    FOREIGN KEY (scan_id, chapter_number) REFERENCES chapters(scan_id, chapter_number) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS profiles (
    id TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    avatar_color TEXT NOT NULL DEFAULT '#3b82f6',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

INSERT INTO profiles(id, display_name, avatar_color)
VALUES('default', 'Default', '#3b82f6')
ON CONFLICT(id) DO NOTHING;

CREATE TABLE IF NOT EXISTS reading_progress (
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    profile TEXT NOT NULL DEFAULT 'default',
    chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (scan_id, profile)
);

CREATE TABLE IF NOT EXISTS favorites (
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    profile TEXT NOT NULL DEFAULT 'default',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (scan_id, profile)
);

CREATE TABLE IF NOT EXISTS bookmarks (
    id BIGSERIAL PRIMARY KEY,
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    profile TEXT NOT NULL DEFAULT 'default',
    chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    note TEXT NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS reading_history (
    id BIGSERIAL PRIMARY KEY,
    scan_id TEXT NOT NULL REFERENCES scans(id) ON DELETE CASCADE,
    profile TEXT NOT NULL DEFAULT 'default',
    chapter_number INTEGER NOT NULL CHECK (chapter_number > 0),
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    read_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS page_texts (
    scan_id TEXT NOT NULL,
    chapter_number INTEGER NOT NULL,
    page_number INTEGER NOT NULL,
    page_text TEXT NOT NULL DEFAULT '',
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (scan_id, chapter_number, page_number),
    FOREIGN KEY (scan_id, chapter_number, page_number) REFERENCES pages(scan_id, chapter_number, page_number) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_pages_scan_chapter ON pages(scan_id, chapter_number, page_number);
CREATE INDEX IF NOT EXISTS idx_history_profile ON reading_history(profile, read_at DESC);
CREATE INDEX IF NOT EXISTS idx_page_texts_scan_chapter ON page_texts(scan_id, chapter_number, page_number);
