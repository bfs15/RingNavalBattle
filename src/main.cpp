#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <queue>

#include "Ship.h"
#include "Board.h"
#include "Network.h"

using namespace std;

void board_setup(Board& board, long numShips){
	Ship newShip;
	
	cout << "Taichou, This is our advanced technology map of the highest precision, the top left coordinate is (0,0) (y,x)\n";
	board.print();
	cout << "You have " << numShips << " to dispatch.\n";
	string confirmed("n");
	while(confirmed != "y"){
		board.set_board(water);
		for(long i = 0; i < numShips; i++){
			cout << "What will be the coord for the top left (y x) of the ship #" << i << " taichou?\n";
			cin >> newShip.top_left.y >> newShip.top_left.x;
		
			cout << "And how about the height and width of the ship #" << i << " taichou?\n";
			cin >> newShip.height >> newShip.width;
			
			board.print();
			if(board.set_ship(newShip) == FAIL){
				i--;
				cout << "You can't set up a ship like that! Try again but pay more attention this time taichou!\n";
			}
		}
		
		confirmed = "";
		cout << "It's not like I like how you set up the board or anything, baka\n";
		cout << "But do you confirm it? (y/n)" << endl;
		cin >> confirmed;
		confirmed.at(0) = tolower(confirmed.at(0));

		while(confirmed != "y" && confirmed != "n"){	//wrong input handler
			confirmed = "";
			cout << "Answer correctly baka! Do you confirm it? (y/n)" << endl;
			cin >> confirmed;
			confirmed.at(0) = tolower(confirmed.at(0));
		}
	}
}

void print_ascii(string filename){
	ifstream in_f;
	in_f.open(filename);
	string buffer;
	while(getline(in_f, buffer)){
		cout << buffer <<"\n";
	}
	in_f.close();
}

void print_game(Board my_board, vector<Board> enemies){
	my_board.print();
	
	for(int i = 0; i < enemies.size(); ++i){
		cout << "Player " << i << endl;
		enemies.at(i).print();
	}
}

void read_attack(Coord& pos, int8_t& dest){
	int d;
	cout << "Which player will you attack?" << endl;
	cin >> d;
	dest = d;
	cout << "Where will you attack next taichou? (y x)" << endl;
	cin >> pos.y >> pos.x;
}

int main(int argc, char **argv)
{
	print_ascii("content/amatsukaze.txt");
	int enemy_n = 1;
	long board_max_y = 5;
	long board_max_x = 5;
	int numShips = 2;
	
	int c;
	int my_id = 0;
	string next_hostname = "";
	
	while (( c = getopt(argc, argv, "p:h:")) != -1){
		switch (c){
			case 'p':
				my_id = stoi(optarg);
				break;
			case 'h':
				next_hostname = optarg;
				break;
			case ':':
			// missing option argument
				fprintf(stderr, "%s: option '-%c' requires an argument\n", argv[0], optopt);
			default:
				fprintf(stderr, "Usage: %s -p player_order -n next_hostname\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	
	if(my_id == 0){
		cout << "-p is mandatory\n";
		exit(1);
	}
	
	if(next_hostname == ""){
		cout << "-h is mandatory\n";
		exit(1);
	}
	
	msg_buffer buf;
	
	sockaddr_in p_addr;
	
	msg_buffer msg_to_send;
	size_t msg_to_send_size;
	
	Board my_board(Coord{.y = board_max_y, .x = board_max_x});
	Ship ship_hit;
	
	vector<Board> enemies;
	
	for (int i = 0; i < enemy_n; ++i){
		enemies.push_back(Board(Coord{.y = board_max_y, .x = board_max_x}, numShips));
	}

	board_setup(my_board, numShips);
	
	print_game(my_board, enemies);
	
	Connection net(my_id, next_hostname); // TODO: real ids
	
	bool has_response = false;
	bool game_ended = false;
	bool my_turn;

	if(net.my_id == 1){
		my_turn = true;
		cout<< "it's your turn" << endl;
	}else{
		my_turn = false;
		cout<< "standby" << endl;
	}
	
	while(!game_ended){
		if(my_turn){
			if(my_board.ship_n > 0){
				msg_buffer attack_msg;
				attack_msg.info = nil_msg;
				Coord pos;
				
				read_attack(pos, attack_msg.info.dest);
				attack_msg.coord_info.coord = pos;
				
				// set up msg info
				attack_msg.info.baton = false;
				attack_msg.info.status = 0;
				attack_msg.info.origin = net.my_id;
				attack_msg.info.content = content_attack;
				
				net.send_msg(&attack_msg, sizeof(coord_msg));
				
				net.pass_baton();
				
				net.rec_msg(&buf);
				// process hit
				if(buf.info.content == content_hit){
					enemies.at(0).at(pos).hit = true;	
					if( available(enemies.at(0).at(pos)) ){
						enemies.at(0).at(pos).idn = unk_ship;
					}
					
				} else if (buf.info.content ==  content_ship_destroyed){
					cout <<  "destroyed enemy ship" << endl;
					
					enemies.at(0).set_destroyed_ship(buf.ship_info.ship);
					if(enemies.at(0).ship_n == 0){
						cout << "Player at ID has been annihilated, who will be next?\n";
					}
				} else {
					cout << "you missed!" << endl;
				}
				cout << "enemy map\n";
				enemies.at(0).print();

				// wait baton
				while(!net.with_baton){
					net.prev_player.rec(&buf, &p_addr);
					net.is_this_for_me(buf);
				}
			}
			
			net.pass_turn(my_turn);
		} else {	//not my turn
			if (net.with_baton){
				if(has_response){ // msg_queue.size() > 0
					//send_msg(msg_queue.front());
					//msg_queue.pop_front();
					net.send_msg(&msg_to_send, msg_to_send_size);
					has_response = false;
				}
				net.pass_baton();
			}else{
				size_t msg_size = net.prev_player.rec(&buf, &p_addr);
				
				print(buf.info);
				
				// Message for me
				if( net.is_this_for_me(buf) ){
					cout << "We received a tegami taichou" << endl;
					buf.info.status = status_ok;
					
					if(buf.info.content == content_attack){
						cout << "We got attacked" << endl;
						
						has_response = true;
						// set up response msg info
						msg_to_send.info = nil_msg;
						msg_to_send.info.dest = buf.info.origin;
						msg_to_send.info.origin = net.my_id;
						
						// calculate hit
						int ship_hp = my_board.attackField(buf.coord_info.coord, ship_hit);
						
						if(ship_hp == 0){
							cout << "They destroyed our ship!" << endl;
							msg_to_send_size = sizeof(ship_msg);
//							for(int i=0; i < enemies.size(); i++){
//								ship_destroyed_msg.info.dest = enemies.at(i).player;
//								msg_queue.push_back(ship_destroyed_msg);
//							}
							msg_to_send.info.content = content_ship_destroyed;
							msg_to_send.ship_info.ship = ship_hit;
						} else {
							msg_to_send_size = sizeof(msg);
							if(ship_hp != FAIL){
								cout << "it hit!" << endl;
								msg_to_send.info.content = content_hit;
							} else {
								cout << "it missed. nyahahah" << endl;
								msg_to_send.info.content = content_miss;
							}
							//msg_queue.push_back(hit_msg);
						}
						cout << "our map:\n";
						my_board.print();
					// if not an attack
					} else if (buf.info.content == content_turn){	//turn
						cout << "It's now our turn!" << endl;
						my_turn = true;
					}
				}
				// send forward non baton msgs
				if( ! buf.info.baton ){
					net.next_player.send(&buf, msg_size);
				}
			}
		}
	}
	
	return 0;
}
