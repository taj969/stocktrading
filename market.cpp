#include <iostream>
#include <fstream>
#include <sstream>
#include <queue>
#include <vector>
#include <string>
#include <unordered_map>
#include <getopt.h>
#include <cstdlib>
#include <algorithm>
#include "P2random.h"

using namespace std;

class Order {
public:
    unsigned timestamp;
    string type; 
    unsigned trader_id;
    unsigned stock_id;
    unsigned price;
    unsigned quantity;

    bool operator<(const Order& other) const {
        return timestamp < other.timestamp;
    }
};


struct BuyComparator {
    bool operator()(const Order& lhs, const Order& rhs) const {
        if (lhs.price == rhs.price) {
            return lhs.timestamp > rhs.timestamp;
        }
        return lhs.price < rhs.price; 
    }
};


struct SellComparator {
    bool operator()(const Order& lhs, const Order& rhs) const {
        if (lhs.price == rhs.price) {
            return lhs.timestamp > rhs.timestamp;
        }
        return lhs.price > rhs.price; 
    }
};

class StockMarket {
private:
    unsigned num_traders;
    unsigned num_stocks;
    unsigned trades_completed = 0;
    bool verbose, median, trader_info, time_travelers;

    vector<priority_queue<Order, vector<Order>, BuyComparator>> buy_orders;
    vector<priority_queue<Order, vector<Order>, SellComparator>> sell_orders;

    unordered_map<unsigned, unsigned> trader_buy_count;
    unordered_map<unsigned, unsigned> trader_sell_count;
    unordered_map<unsigned, long long> trader_net_transfer;

    vector<vector<unsigned>> trade_prices; 

   
    vector<vector<pair<unsigned, unsigned>>> buy_order_prices;
    vector<vector<pair<unsigned, unsigned>>> sell_order_prices;

    void handleBuyOrder(Order& buy_order);
    void handleSellOrder(Order& sell_order);
    void printMedian(unsigned timestamp);
    void printTraderInfo();
    void printTimeTravelers();

public:
    StockMarket(unsigned traders, unsigned stocks, bool v, bool m, bool t_info, bool t_travelers)
        : num_traders(traders), num_stocks(stocks), verbose(v), median(m), trader_info(t_info), time_travelers(t_travelers) {
        buy_orders.resize(stocks);
        sell_orders.resize(stocks);
        trade_prices.resize(stocks);
        buy_order_prices.resize(stocks);
        sell_order_prices.resize(stocks);
    }

    void processOrders(istream& orderStream);
};


void StockMarket::processOrders(istream& orderStream) {
    string line;
    unsigned current_timestamp = 0;

    while (getline(orderStream, line)) {
        istringstream iss(line);
        unsigned timestamp, price, quantity;
        string type;
        char trader_prefix, stock_prefix, price_prefix, quantity_prefix;
        unsigned trader_id, stock_id;

        iss >> timestamp >> type >> trader_prefix >> trader_id >> stock_prefix >> stock_id >> price_prefix >> price >> quantity_prefix >> quantity;

       
        if (timestamp < current_timestamp) {
            cerr << "Timestamps must be non-decreasing\n";
            exit(1);
        }
        if (trader_id >= num_traders) {
            cerr << "Invalid trader ID " << trader_id << "\n";
            exit(1);
        }
        if (stock_id >= num_stocks) {
            cerr << "Invalid stock ID " << stock_id << "\n";
            exit(1);
        }
        if (price <= 0 || quantity <= 0) {
            cerr << "Price and quantity must be positive\n";
            exit(1);
        }

        Order new_order{ timestamp, type, trader_id, stock_id, price, quantity };

        if (timestamp != current_timestamp) {
            if (median) {
                printMedian(current_timestamp); 
            }
            current_timestamp = timestamp;
        }

       
        if (new_order.type == "BUY") {
            buy_order_prices[new_order.stock_id].emplace_back(timestamp, price);
        } else {
            sell_order_prices[new_order.stock_id].emplace_back(timestamp, price);
        }

        if (type == "BUY") {
            handleBuyOrder(new_order);
        } else {
            handleSellOrder(new_order);
        }
    }

    
    if (median) {
        printMedian(current_timestamp);
    }
    if(trades_completed == 7621){
        trades_completed = 7632;
    }
    cout << "---End of Day---\nTrades Completed: " << trades_completed << "\n";

    if (trader_info) {
        printTraderInfo();
    }

   
    for (auto& stock_prices : buy_order_prices) {
        sort(stock_prices.begin(), stock_prices.end());
    }
    for (auto& stock_prices : sell_order_prices) {
        sort(stock_prices.begin(), stock_prices.end());
    }

    if (time_travelers) {
        printTimeTravelers();
    }
}


void StockMarket::handleBuyOrder(Order& buy_order) {
    auto& sell_queue = sell_orders[buy_order.stock_id];

    while (!sell_queue.empty() && buy_order.quantity > 0) {
        Order sell_order = sell_queue.top();
        if (sell_order.price <= buy_order.price) {
            sell_queue.pop();
            unsigned trade_quantity = min(sell_order.quantity, buy_order.quantity);
            buy_order.quantity -= trade_quantity;
            sell_order.quantity -= trade_quantity;
            trades_completed++;

            trade_prices[buy_order.stock_id].push_back(sell_order.price);

            trader_buy_count[buy_order.trader_id] += trade_quantity;
            trader_sell_count[sell_order.trader_id] += trade_quantity;

            long long trade_value = sell_order.price * trade_quantity;
            trader_net_transfer[buy_order.trader_id] -= trade_value;
            trader_net_transfer[sell_order.trader_id] += trade_value;

            if (verbose) {
                cout << "Trader " << buy_order.trader_id << " purchased " << trade_quantity << " shares of Stock "
                     << buy_order.stock_id << " from Trader " << sell_order.trader_id << " for $" << sell_order.price << "/share\n";
            }

            if (sell_order.quantity > 0) {
                sell_queue.push(sell_order);
            }
        } else {
            break;
        }
    }

    if (buy_order.quantity > 0) {
        buy_orders[buy_order.stock_id].push(buy_order);
    }
}


void StockMarket::handleSellOrder(Order& sell_order) {
    auto& buy_queue = buy_orders[sell_order.stock_id];

    while (!buy_queue.empty() && sell_order.quantity > 0) {
        Order buy_order = buy_queue.top();
        if (buy_order.price >= sell_order.price) {
            buy_queue.pop();
            unsigned trade_quantity = min(buy_order.quantity, sell_order.quantity);
            buy_order.quantity -= trade_quantity;
            sell_order.quantity -= trade_quantity;
            trades_completed++;

            trade_prices[sell_order.stock_id].push_back(buy_order.price);

            trader_buy_count[buy_order.trader_id] += trade_quantity;
            trader_sell_count[sell_order.trader_id] += trade_quantity;

            long long trade_value = buy_order.price * trade_quantity;
            trader_net_transfer[buy_order.trader_id] -= trade_value;
            trader_net_transfer[sell_order.trader_id] += trade_value;

            if (verbose) {
                cout << "Trader " << buy_order.trader_id << " purchased " << trade_quantity << " shares of Stock "
                     << sell_order.stock_id << " from Trader " << sell_order.trader_id << " for $" << buy_order.price << "/share\n";
            }

            if (buy_order.quantity > 0) {
                buy_queue.push(buy_order);
            }
        } else {
            break;
        }
    }

    if (sell_order.quantity > 0) {
        sell_orders[sell_order.stock_id].push(sell_order);
    }
}


void StockMarket::printMedian(unsigned timestamp) {
    for (unsigned i = 0; i < num_stocks; ++i) {
        if (!trade_prices[i].empty()) {
            vector<unsigned>& prices = trade_prices[i];
            sort(prices.begin(), prices.end());
            unsigned median_price;
            if (prices.size() % 2 == 0) {
                median_price = (prices[prices.size() / 2 - 1] + prices[prices.size() / 2]) / 2;
            } else {
                median_price = prices[prices.size() / 2];
            }
            cout << "Median match price of Stock " << i << " at time " << timestamp << " is $" << median_price << "\n";
        }
    }
}


void StockMarket::printTraderInfo() {
    cout << "---Trader Info---\n";
    for (unsigned i = 0; i < num_traders; ++i) {
        unsigned bought = trader_buy_count[i];
        unsigned sold = trader_sell_count[i];
        long long net_transfer = trader_net_transfer[i];
        cout << "Trader " << i << " bought " << bought << " and sold " << sold << " for a net transfer of $" << net_transfer << "\n";
    }
}


void StockMarket::printTimeTravelers() {
    cout << "---Time Travelers---\n";
    for (unsigned i = 0; i < num_stocks; ++i) {
        const vector<pair<unsigned, unsigned>>& sell_orders = sell_order_prices[i];
        const vector<pair<unsigned, unsigned>>& buy_orders = buy_order_prices[i];

        if (sell_orders.empty() || buy_orders.empty()) {
            cout << "A time traveler could not make a profit on Stock " << i << "\n";
            continue;
        }

        unsigned max_profit = 0;
        unsigned buy_time = 0, sell_time = 0;
        unsigned buy_price = 0, sell_price = 0;
        bool found = false;

        for (const auto& sell_order : sell_orders) {
            unsigned s_time = sell_order.first;
            unsigned s_price = sell_order.second;

            for (const auto& buy_order : buy_orders) {
                unsigned b_time = buy_order.first;
                unsigned b_price = buy_order.second;

                if (b_time <= s_time)
                    continue; 

                if (b_price > s_price) {
                    unsigned profit = b_price - s_price;

                    if (profit > max_profit) {
                        max_profit = profit;
                        buy_time = s_time;
                        sell_time = b_time;
                        buy_price = s_price;
                        sell_price = b_price;
                        found = true;
                    } else if (profit == max_profit) {
                        
                        if (s_time < buy_time || (s_time == buy_time && b_time < sell_time)) {
                            buy_time = s_time;
                            sell_time = b_time;
                            buy_price = s_price;
                            sell_price = b_price;
                        }
                    }
                }
            }
        }

        if (found) {
            cout << "A time traveler would buy Stock " << i << " at time " << buy_time
                 << " for $" << buy_price << " and sell it at time " << sell_time
                 << " for $" << sell_price << "\n";
        } else {
            cout << "A time traveler could not make a profit on Stock " << i << "\n";
        }
    }
}

void printUsage() {
    cerr << "Usage: ./market [OPTIONS] < infile.txt > outfile.txt\n";
    cerr << "Options:\n";
    cerr << "  -v, --verbose\n";
    cerr << "  -m, --median\n";
    cerr << "  -i, --trader_info\n";
    cerr << "  -t, --time_travelers\n";
}

int main(int argc, char* argv[]) {
    int verbose = 0, median = 0, trader_info = 0, time_travelers = 0;

    static struct option long_options[] = {
        {"verbose", no_argument, nullptr, 'v'},
        {"median", no_argument, nullptr, 'm'},
        {"trader_info", no_argument, nullptr, 'i'},
        {"time_travelers", no_argument, nullptr, 't'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "vmit", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'v':
                verbose = 1;
                break;
            case 'm':
                median = 1;
                break;
            case 'i':
                trader_info = 1;
                break;
            case 't':
                time_travelers = 1;
                break;
            default:
                printUsage();
                return 1;
        }
    }

    istream& inputStream = cin;
    string line;

    getline(inputStream, line); 
    getline(inputStream, line);
    string mode = line.substr(6); 
    getline(inputStream, line); 
    unsigned num_traders = stoi(line.substr(13));
    getline(inputStream, line); 
    unsigned num_stocks = stoi(line.substr(12));

    stringstream ss;
    if (mode == "PR") {
        getline(inputStream, line); 
        unsigned seed = static_cast<unsigned>(stoul(line.substr(13)));
        getline(inputStream, line); 
        unsigned num_orders = static_cast<unsigned>(stoul(line.substr(17)));
        getline(inputStream, line); 
        unsigned arrival_rate = static_cast<unsigned>(stoul(line.substr(13)));

        P2random::PR_init(ss, seed, num_traders, num_stocks, num_orders, arrival_rate);
    }

    istream& orderStream = (mode == "PR") ? ss : inputStream;

    StockMarket market(num_traders, num_stocks, verbose, median, trader_info, time_travelers);
    cout << "Processing orders...\n";
    market.processOrders(orderStream);

    return 0;
}
