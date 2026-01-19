#!/usr/bin/env python3
"""
Open Trivia Database Scraper
Scrapes questions from opentdb.com and saves them to JSON files.
Respects rate limits (5 second delay between requests).
"""

import json
import time
import requests
import html
import hashlib
from pathlib import Path

# API endpoints
BASE_URL = "https://opentdb.com/api.php"
TOKEN_URL = "https://opentdb.com/api_token.php"
CATEGORY_URL = "https://opentdb.com/api_category.php"

# Rate limit: 5 seconds between requests
RATE_LIMIT_DELAY = 5.5  # Adding a bit of buffer

# Categories from the API
CATEGORIES = {
    9: "General Knowledge",
    10: "Entertainment: Books",
    11: "Entertainment: Film",
    12: "Entertainment: Music",
    13: "Entertainment: Musicals & Theatres",
    14: "Entertainment: Television",
    15: "Entertainment: Video Games",
    16: "Entertainment: Board Games",
    17: "Science & Nature",
    18: "Science: Computers",
    19: "Science: Mathematics",
    20: "Mythology",
    21: "Sports",
    22: "Geography",
    23: "History",
    24: "Politics",
    25: "Art",
    26: "Celebrities",
    27: "Animals",
    28: "Vehicles",
    29: "Entertainment: Comics",
    30: "Science: Gadgets",
    31: "Entertainment: Japanese Anime & Manga",
    32: "Entertainment: Cartoon & Animations",
}

DIFFICULTIES = ["easy", "medium", "hard"]


def get_session_token():
    """Get a session token to avoid duplicate questions."""
    response = requests.get(TOKEN_URL, params={"command": "request"})
    data = response.json()
    if data["response_code"] == 0:
        print(f"Got session token: {data['token'][:20]}...")
        return data["token"]
    else:
        print("Failed to get session token, continuing without one")
        return None


def reset_token(token):
    """Reset a session token when it's exhausted."""
    response = requests.get(TOKEN_URL, params={"command": "reset", "token": token})
    data = response.json()
    return data["response_code"] == 0


def generate_question_id(question_data):
    """Generate a unique ID based on question content."""
    content = question_data["question"] + "".join(sorted(question_data.get("incorrect_answers", [])))
    return hashlib.md5(content.encode()).hexdigest()[:12]


def decode_html_entities(obj):
    """Recursively decode HTML entities in strings."""
    if isinstance(obj, str):
        return html.unescape(obj)
    elif isinstance(obj, list):
        return [decode_html_entities(item) for item in obj]
    elif isinstance(obj, dict):
        return {key: decode_html_entities(value) for key, value in obj.items()}
    return obj


def fetch_questions(amount=50, category=None, difficulty=None, token=None):
    """Fetch questions from the API."""
    params = {
        "amount": amount,
        "type": "multiple",  # Only multiple choice
        "encode": "url3986",  # URL encoding to avoid HTML entity issues
    }

    if category:
        params["category"] = category
    if difficulty:
        params["difficulty"] = difficulty
    if token:
        params["token"] = token

    try:
        response = requests.get(BASE_URL, params=params, timeout=30)
        data = response.json()

        # Decode URL-encoded strings
        if data["response_code"] == 0:
            from urllib.parse import unquote
            for q in data["results"]:
                q["question"] = unquote(q["question"])
                q["correct_answer"] = unquote(q["correct_answer"])
                q["incorrect_answers"] = [unquote(a) for a in q["incorrect_answers"]]
                q["category"] = unquote(q["category"])

        return data
    except Exception as e:
        print(f"Error fetching questions: {e}")
        return {"response_code": -1, "results": []}


def scrape_all_questions(output_dir="scraped_questions"):
    """Scrape questions from all categories and difficulties."""
    output_path = Path(output_dir)
    output_path.mkdir(exist_ok=True)

    all_questions = []
    seen_ids = set()

    # Get session token
    token = get_session_token()
    time.sleep(RATE_LIMIT_DELAY)

    total_fetched = 0

    # Scrape each category and difficulty
    for cat_id, cat_name in CATEGORIES.items():
        for difficulty in DIFFICULTIES:
            print(f"\nFetching: {cat_name} - {difficulty}")

            # Try to fetch 50 questions (max per request)
            data = fetch_questions(
                amount=50,
                category=cat_id,
                difficulty=difficulty,
                token=token
            )

            if data["response_code"] == 0:
                for q in data["results"]:
                    q_id = generate_question_id(q)

                    if q_id not in seen_ids:
                        seen_ids.add(q_id)

                        # Add our generated ID
                        q["id"] = q_id
                        all_questions.append(q)
                        total_fetched += 1

                print(f"  Got {len(data['results'])} questions (total unique: {total_fetched})")

            elif data["response_code"] == 1:
                print(f"  Not enough questions available for this combination")

            elif data["response_code"] == 4:
                print(f"  Token exhausted, resetting...")
                if token:
                    reset_token(token)
                    time.sleep(RATE_LIMIT_DELAY)
                    # Retry this request
                    data = fetch_questions(
                        amount=50,
                        category=cat_id,
                        difficulty=difficulty,
                        token=token
                    )
                    if data["response_code"] == 0:
                        for q in data["results"]:
                            q_id = generate_question_id(q)
                            if q_id not in seen_ids:
                                seen_ids.add(q_id)
                                q["id"] = q_id
                                all_questions.append(q)
                                total_fetched += 1
                        print(f"  Got {len(data['results'])} questions after reset")
            else:
                print(f"  Error response code: {data['response_code']}")

            # Respect rate limit
            time.sleep(RATE_LIMIT_DELAY)

    # Save all questions to a single JSON file
    all_output = output_path / "all_questions.json"
    with open(all_output, "w", encoding="utf-8") as f:
        json.dump({
            "total_questions": len(all_questions),
            "categories": list(CATEGORIES.values()),
            "difficulties": DIFFICULTIES,
            "questions": all_questions
        }, f, indent=2, ensure_ascii=False)

    print(f"\n{'='*50}")
    print(f"Scraping complete!")
    print(f"Total unique questions: {len(all_questions)}")
    print(f"Saved to: {all_output}")

    # Also save per-category files for the flashcard plugin format
    save_flashcard_format(all_questions, output_path)

    return all_questions


def save_flashcard_format(questions, output_path):
    """Save questions in the flashcard plugin format, organized by category."""

    # Group by category
    by_category = {}
    for q in questions:
        cat = q["category"]
        if cat not in by_category:
            by_category[cat] = []
        by_category[cat].append(q)

    flashcard_dir = output_path / "flashcard_format"
    flashcard_dir.mkdir(exist_ok=True)

    for category, cat_questions in by_category.items():
        # Create safe filename
        safe_name = category.replace(": ", "_").replace(" ", "_").replace("&", "and")

        # Convert to flashcard format
        flashcard_questions = []
        for q in cat_questions:
            # Combine correct and incorrect answers, shuffle would happen at runtime
            all_options = q["incorrect_answers"] + [q["correct_answer"]]
            # Sort alphabetically for consistency
            all_options.sort()

            flashcard_questions.append({
                "id": q["id"],
                "question": q["question"],
                "options": all_options,
                "answer": q["correct_answer"],
                "difficulty": q["difficulty"],
                "category": q["category"]
            })

        # Save category file
        cat_file = flashcard_dir / f"{safe_name}.json"
        with open(cat_file, "w", encoding="utf-8") as f:
            json.dump({
                "category": category,
                "question_count": len(flashcard_questions),
                "questions": flashcard_questions
            }, f, indent=2, ensure_ascii=False)

        print(f"Saved {len(flashcard_questions)} questions to {cat_file.name}")


def scrape_quick(num_requests=10):
    """Quick scrape - just fetch a smaller number of questions for testing."""
    output_path = Path("scraped_questions")
    output_path.mkdir(exist_ok=True)

    all_questions = []
    seen_ids = set()

    token = get_session_token()
    time.sleep(RATE_LIMIT_DELAY)

    for i in range(num_requests):
        print(f"\nRequest {i+1}/{num_requests}")

        data = fetch_questions(amount=50, token=token)

        if data["response_code"] == 0:
            for q in data["results"]:
                q_id = generate_question_id(q)
                if q_id not in seen_ids:
                    seen_ids.add(q_id)
                    q["id"] = q_id
                    all_questions.append(q)

            print(f"  Got {len(data['results'])} questions (total unique: {len(all_questions)})")
        else:
            print(f"  Error: response code {data['response_code']}")

        if i < num_requests - 1:
            time.sleep(RATE_LIMIT_DELAY)

    # Save results
    output_file = output_path / "quick_scrape.json"
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump({
            "total_questions": len(all_questions),
            "questions": all_questions
        }, f, indent=2, ensure_ascii=False)

    print(f"\nSaved {len(all_questions)} questions to {output_file}")

    save_flashcard_format(all_questions, output_path)

    return all_questions


def scrape_category_range(cat_ids, output_file):
    """Scrape specific categories and save to a single output file."""
    all_questions = []
    seen_ids = set()

    token = get_session_token()
    time.sleep(RATE_LIMIT_DELAY)

    for cat_id in cat_ids:
        cat_name = CATEGORIES.get(cat_id, f"Category {cat_id}")
        for difficulty in DIFFICULTIES:
            print(f"[{output_file}] Fetching: {cat_name} - {difficulty}")

            data = fetch_questions(
                amount=50,
                category=cat_id,
                difficulty=difficulty,
                token=token
            )

            if data["response_code"] == 0:
                for q in data["results"]:
                    q_id = generate_question_id(q)
                    if q_id not in seen_ids:
                        seen_ids.add(q_id)
                        q["id"] = q_id
                        all_questions.append(q)
                print(f"  Got {len(data['results'])} questions (total: {len(all_questions)})")
            elif data["response_code"] == 1:
                print(f"  Not enough questions for this combination")
            elif data["response_code"] == 4:
                print(f"  Token exhausted, resetting...")
                if token:
                    reset_token(token)
                    time.sleep(RATE_LIMIT_DELAY)

            time.sleep(RATE_LIMIT_DELAY)

    # Save to output file
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with open(output_path, "w", encoding="utf-8") as f:
        json.dump({
            "total_questions": len(all_questions),
            "categories": [CATEGORIES.get(c, str(c)) for c in cat_ids],
            "questions": all_questions
        }, f, indent=2, ensure_ascii=False)

    print(f"\n[{output_file}] Saved {len(all_questions)} questions")
    return all_questions


def merge_scraped_files(input_files, output_file):
    """Merge multiple scraped JSON files into one."""
    all_questions = []
    seen_ids = set()
    all_categories = set()

    for input_file in input_files:
        try:
            with open(input_file, "r", encoding="utf-8") as f:
                data = json.load(f)

            for q in data.get("questions", []):
                q_id = q.get("id", generate_question_id(q))
                if q_id not in seen_ids:
                    seen_ids.add(q_id)
                    all_questions.append(q)

            for cat in data.get("categories", []):
                all_categories.add(cat)

            print(f"Loaded {len(data.get('questions', []))} questions from {input_file}")
        except Exception as e:
            print(f"Error loading {input_file}: {e}")

    # Save merged output
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump({
            "total_questions": len(all_questions),
            "categories": sorted(list(all_categories)),
            "difficulties": DIFFICULTIES,
            "questions": all_questions
        }, f, indent=2, ensure_ascii=False)

    print(f"\nMerged {len(all_questions)} unique questions into {output_file}")

    # Also save in flashcard format
    output_path = Path(output_file).parent
    save_flashcard_format(all_questions, output_path)

    return all_questions


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Scrape Open Trivia Database")
    parser.add_argument("--quick", type=int, metavar="N",
                        help="Quick scrape: just N requests of 50 questions each")
    parser.add_argument("--full", action="store_true",
                        help="Full scrape: all categories and difficulties")
    parser.add_argument("--output", "-o", default="scraped_questions",
                        help="Output directory (default: scraped_questions)")
    parser.add_argument("--categories", type=str,
                        help="Comma-separated list of category IDs to scrape (e.g., '9,10,11')")
    parser.add_argument("--output-file", type=str,
                        help="Output JSON file for category scrape")
    parser.add_argument("--merge", nargs="+",
                        help="Merge multiple JSON files into one")
    parser.add_argument("--merge-output", type=str, default="scraped_questions/merged.json",
                        help="Output file for merged results")

    args = parser.parse_args()

    if args.merge:
        merge_scraped_files(args.merge, args.merge_output)
    elif args.categories and args.output_file:
        cat_ids = [int(c.strip()) for c in args.categories.split(",")]
        print(f"Scraping categories: {cat_ids}")
        scrape_category_range(cat_ids, args.output_file)
    elif args.quick:
        print(f"Quick scrape mode: {args.quick} requests")
        scrape_quick(args.quick)
    elif args.full:
        print("Full scrape mode: all categories and difficulties")
        print("This will take approximately", len(CATEGORIES) * len(DIFFICULTIES) * RATE_LIMIT_DELAY / 60, "minutes")
        scrape_all_questions(args.output)
    else:
        # Default: do a reasonable scrape (10 requests = ~500 questions)
        print("Default mode: 10 requests (~500 questions)")
        print("Use --full for complete scrape or --quick N for custom amount")
        scrape_quick(10)
