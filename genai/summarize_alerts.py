import os
import json
from kafka import KafkaConsumer
from dotenv import load_dotenv

from langchain.prompts import PromptTemplate
from langchain_google_genai import ChatGoogleGenerativeAI
from langchain.chains import LLMChain

load_dotenv()
api_key = os.getenv("GEMINI_API_KEY")
if not api_key:
    raise ValueError("GEMINI_API_KEY not found in .env")

llm = ChatGoogleGenerativeAI(
    model="models/gemini-1.5-flash-latest",
    google_api_key=api_key,
    temperature=0.3
)

prompt = PromptTemplate(
    input_variables=["alert_json"],
    template="""
You are an intelligent trading assistant. Explain this anomaly alert JSON in simple terms:
{alert_json}

Be concise, max 3 lines.
"""
)

chain = LLMChain(llm=llm, prompt=prompt)

consumer = KafkaConsumer(
    "alerts",
    bootstrap_servers="localhost:9092",
    auto_offset_reset="latest",
    enable_auto_commit=True,
    group_id="langchain-gemini",
    value_deserializer=lambda m: m.decode("utf-8")
)

print("--- Gemini alert summarizer is now running... ---\n")

for message in consumer:
    alert_json = message.value
    try:
        parsed = json.loads(alert_json)
        explanation = chain.run(alert_json=alert_json)
        print("Raw Alert:\n", json.dumps(parsed, indent=2))
        print("Gemini Explanation:\n", explanation)
        print("=" * 60 + "\n")
    except Exception as e:
        print("Failed to process message:", e)
