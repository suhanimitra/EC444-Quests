import json
import streamlit as st
from tingodb import TinyDB, Query
from my_tingodb_wrapper import connect_to_db, get_data

def connect_to_db(db_path):
  return TinyDB(db_path)

def get_data(db, query):
  return db.search(query)

st.title("TingoDB Data")
# Fetch data and display
data = get_data(db, Query().name == 'John')
st.dataframe(data)