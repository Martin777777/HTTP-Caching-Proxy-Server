from flask import Flask, make_response

app = Flask(__name__)

@app.route('/expire-in-seconds')
def expire_in_seconds():
    response = make_response("This content expires in 10 seconds.")
    response.headers['Cache-Control'] = 'max-age=10'
    return response

@app.route('/need-revalidate')
def must_revalidate():
    response = make_response("This content must be revalidated.")
    response.headers['Cache-Control'] = 'no-cache'
    return response

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
