import os

from env_config import load_dotenv_if_present


def check_env_vars():
    load_dotenv_if_present()
    required_vars = ["TELEGRAM_BOT_TOKEN", "TELEGRAM_CHAT_ID"]
    missing_vars = []

    for var in required_vars:
        if not os.environ.get(var):
            missing_vars.append(var)

    if missing_vars:
        print(f"Error: The following environment variables are not set: {', '.join(missing_vars)}")
        return False
    else:
        print("All required environment variables are set.")
        return True

if __name__ == "__main__":
    check_env_vars()
