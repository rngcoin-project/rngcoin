#include <string>
#include <map>
#include <vector>

class UniValue;

using namespace std;

//-----------------------------------------------------
// https request using libevent. Body in the https-cli.cpp
// ATTN: Function is NOT REENTERABLE
// Returns HTTP status code if OK, or -1 if error
// Ret contains server answer (if OK), orr error text (-1)
int HttpsLE(const char *host, const char *get, const char *post, const std::map<std::string,std::string> &header, std::string *ret);

//-----------------------------------------------------
class Exch {
  public:
  Exch(const string &retAddr); 
  virtual ~Exch() {};

  virtual const string& Name() const = 0;
  virtual const string& Host() const = 0;
  
  // Get currency for exchnagge to, like btc, ltc, etc
  // Fill MarketInfo from exchange.
  // Returns the empty string if OK, or error message, if error
  virtual string MarketInfo(const string &currency, double amount) = 0;

  // Create SEND exchange channel for 
  // Send "amount" in external currecny "to" address
  // Fills m_depAddr..m_txKey, and updates m_rate
  // Returns error text, or an empty string, if OK
  virtual string Send(const string &to, double amount) = 0;

  // Check status of existing transaction.
  // If key is empty, used the last key
  // Returns status (including err), or minus "-", if "not my" key
  virtual string TxStat(const string &txkey, UniValue &details) = 0;

  // Cancel TX by txkey.
  // If key is empty, used the last key
  // Returns error text, or an empty string, if OK
  // Returns minus "-", if "not my" key
  virtual string Cancel(const string &txkey) = 0;

  // Check time in secs, remain in the contract, created by prev Send()
  // If key is empty, used the last key
  // Returns time or zero, if contract expired
  // Returns -1, if "not my" key
  virtual int Remain(const string &txkey) = 0;


  // Returns extimated RNG to pay for specific pay_amount
  // Must be called after MarketInfo
  double EstimatedRNG(double pay_amount) const;

  string m_retAddr; // Return RNG Addr

  // MarketInfo fills these params
  string m_pair;
  double m_rate;
  double m_limit;
  double m_min;
  double m_minerFee;

  // Send fills these params + m_rate above
  string m_depAddr;	// Address to pay RNG
  string m_outAddr;	// Address to pay from exchange
  double m_depAmo;	// amount in RNG
  double m_outAmo;	// Amount transferred to BTC
  string m_txKey;	// TX reference key

  protected:
  // Fill exchange-specific fields into m_header for future https request
  virtual void FillHeader() {}
  // Connect to the server by https, fetch JSON and parse to UniValue
  // Throws exception if error
  UniValue httpsFetch(const char *get, const UniValue *post);

  // Get input path within server, like: /api/marketinfo/rng_btc.json
  // Called from exchange-specific MarketInfo()
  // Fill MarketInfo from exchange.
  // Throws exception if error
  const UniValue RawMarketInfo(const string &path);

  // Check JSON-answer for "error" key, and throw error
  // message, if exists
  virtual void CheckERR(const UniValue &reply) const;

  // Extract raw key from txkey
  // Return NULL if "Not my key" or invalid key
  const char *RawKey(const string &txkey) const;

  // extra header fields, needed for some exchange
  std::map<std::string,std::string> m_header;

}; // class Exch

//-----------------------------------------------------
class ExchCoinReform : public Exch {
  public:
  ExchCoinReform(const string &retAddr);

  virtual ~ExchCoinReform();

  virtual const string& Name() const;
  virtual const string& Host() const;

  // Get currency for exchnagge to, like btc, ltc, etc
  // Fill MarketInfo from exchange.
  // Returns the empty string if OK, or error message, if error
  virtual string MarketInfo(const string &currency, double amount);

  // Creatse SEND exchange channel for 
  // Send "amount" in external currecny "to" address
  // Fills m_depAddr..m_txKey, and updates m_rate
  virtual string Send(const string &to, double amount);

  // Check status of existing transaction.
  // If key is empty, used the last key
  // Returns status (including err), or minus "-", if "not my" key
  virtual string TxStat(const string &txkey, UniValue &details);

  // Cancel TX by txkey.
  // If key is empty, used the last key
  // Returns error text, or an empty string, if OK
  // Returns minus "-", if "not my" key
  virtual string Cancel(const string &txkey);

  // Check time in secs, remain in the contract, created by prev Send()
  // If key is empty, used the last key
  // Returns time or zero, if contract expired
  // Returns -1, if "not my" key
  virtual int Remain(const string &txkey);

}; // class ExchCoinReform

//-----------------------------------------------------
class ExchCoinSwitch : public Exch {
  public:
  ExchCoinSwitch(const string &retAddr);

  virtual ~ExchCoinSwitch();

  virtual const string& Name() const;
  virtual const string& Host() const;

  // Get currency for exchnagge to, like btc, ltc, etc
  // Fill MarketInfo from exchange.
  // Returns the empty string if OK, or error message, if error
  virtual string MarketInfo(const string &currency, double amount);

  // Creatse SEND exchange channel for 
  // Send "amount" in external currecny "to" address
  // Fills m_depAddr..m_txKey, and updates m_rate
  virtual string Send(const string &to, double amount);

  // Check status of existing transaction.
  // If key is empty, used the last key
  // Returns status (including err), or minus "-", if "not my" key
  virtual string TxStat(const string &txkey, UniValue &details);

  // Cancel TX by txkey.
  // If key is empty, used the last key
  // Returns error text, or an empty string, if OK
  // Returns minus "-", if "not my" key
  virtual string Cancel(const string &txkey);

  // Check time in secs, remain in the contract, created by prev Send()
  // If key is empty, used the last key
  // Returns time or zero, if contract expired
  // Returns -1, if "not my" key
  virtual int Remain(const string &txkey);

  private:
  // Fill exchange-specific fields into m_header for future https request
  virtual void FillHeader();
}; // class ExchCoinSwitch


//-----------------------------------------------------
class ExchBox {
  public:
  ExchBox();
  ~ExchBox();
  void Reset(const string &retAddr);

  vector<Exch*> m_v_exch;
}; // class exchbox

//-----------------------------------------------------
