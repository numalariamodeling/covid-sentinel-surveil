#ifndef ED_NUCOVID_H
#define ED_NUCOVID_H

#include <stdlib.h>
#include <vector>
#include <iostream>
#include <iomanip>
#include <queue>
#include "Utility.h"
#include <climits>

using namespace std;


typedef enum {
    SUSCEPTIBLE, 
    EXPOSED, 
    ASYMPTOMATIC, 
    PRESYMPTOMATIC, 
    SYMPTOMATIC_MILD,
    SYMPTOMATIC_SEVERE,
    HOSPITALIZED,
    HOSPITALIZED_CRIT,
    CRITICAL,
    DEATH,
    RESISTANT, 
    STATE_SIZE // STATE_SIZE must be last
} stateType;

typedef enum {
    PRE, ASY, SYMM, SYMS, HOS, CRI, HPC, DEA, RECA, RECM, RECH, RECC, CON, IMM
} eventType;

class Node {
    public:
        int id;
        int N;                      // population size
        vector<double> Ki;          // param for exponential exposed duration
        double Kasym;               // param for exponential time to infectious (asymptomatic)
        double Kpres;               // param for exponential time to infectious (presymptomatic)
        double Kmild;               // param for exponential time to mild from presymp
        double Ksevere;             // param for exponential time to severe from presymp 
        double Khosp;               // param for exponential time to hospitalized from severe 
        double Kcrit;               // param for exponential time to critical from hospitalized 
        double Kdeath;              // param for exponential time to death from critical
        vector<vector<double>> Krec;// param for exponential time to recovery (A, Sm, H, C, HPC)
        vector<double> Pcrit;       // probability of critical given hospitalized
        vector<double> Pdeath;      // probability of death given critical
        vector<vector<double>> Pdetect; // probability of detection (A, P, Sm, Ss)
        double frac_infectiousness_As;  // infectiousness multiplier for As
        double frac_infectiousness_det; // infectiousness multiplier for detected
        vector<int> state_counts;   // S, E, I, R counts
        size_t cumu_symptomatic;
        size_t introduced;

        // constructor
        Node( int ii, int n, vector<double> ki, double ka, double kp, double km, double ks, 
              double kh, double kc, double kd, vector<vector<double>> kr,
              vector<double> pc, vector<double> pd, vector<vector<double>> pdets,
              double fia, double fid) {

            id = ii;
            N = n;
            Ki = ki;
            Kasym = ka;
            Kpres = kp; 
            Kmild = km;
            Ksevere = ks;
            Khosp = kh;
            Kcrit = kc;
            Kdeath = kd;
            Krec = kr;
            Pcrit = pc;
            Pdeath = pd;
            Pdetect = pdets;
            cumu_symptomatic = 0;
            introduced = 0;

            frac_infectiousness_As = fia;
            frac_infectiousness_det = fid;
        }

        void reset() {
            state_counts.clear();
            state_counts.resize(STATE_SIZE, 0);
            state_counts[SUSCEPTIBLE] = N;
        }

        double get_Ki(int day) { return day < Ki.size() ? Ki[day] : Ki[Ki.size() - 1]; }
        double get_Pdet(int day, int ind) { return day < Pdetect.size() ? Pdetect[day][ind] : Pdetect[Pdetect.size() - 1][ind]; }
        double get_Pcrit(int day) { return day < Pcrit.size() ? Pcrit[day] : Pcrit[Pcrit.size() - 1]; }
        double get_Pdeath(int day) { return day < Pdeath.size() ? Pdeath[day] : Pdeath[Pdeath.size() - 1]; }
        double get_Krec(int day, int ind) { return day < Krec.size() ? Krec[day][ind] : Krec[Krec.size() - 1][ind]; }
};

class Event {
    public:
        double time;
        eventType type;
        Node* source_node;
        Node* target_node;
        Event(const Event& o) {  time=o.time; type=o.type; source_node=o.source_node; target_node=o.target_node;}
        Event(double t, eventType e, Node* sn, Node* tn) {time=t; type=e; source_node=sn; target_node=tn;}
        Event& operator=(const Event& o) { time=o.time; type=o.type; source_node=o.source_node; target_node=o.target_node; return *this; }
};

class compTime {
    public:
        bool operator() (const Event* lhs, const Event* rhs) const {
            return (lhs->time>rhs->time);
        }

        bool operator() (const Event& lhs, const Event& rhs) const {
            return (lhs.time>rhs.time);
        }
};

class Event_Driven_NUCOVID {
    public:
        vector<Node*> nodes;
        vector<vector<double>> infection_matrix;
        priority_queue<Event, vector<Event>, compTime > EventQ; // event queue
        double Now;                 // Current "time" in simulation
        mt19937 rng;              // RNG
        
        Event_Driven_NUCOVID (vector<Node*> ns, vector<vector<double>> mat) {
            nodes = ns;
            infection_matrix = mat;
            
            // integrity check nodes vs infection_matrix
            assert(mat.size() == ns.size());
            for (size_t i = 0; i < mat.size(); i++) {
                assert(mat[i].size() == ns.size());
            }
            reset();
        }

        void run_simulation(double duration) {
            double start_time = Now;
            int day = (int) Now;
            cout << setprecision(3) << fixed;
            cout << "node\ttime\tKi\tS\tE\tAP\tSYM\tHOS\tCRIT\tDEA\tR\tintroduced" << endl; 
            while (next_event() and Now < start_time + duration) {
                if ((int) Now > day) {
                    for (size_t i = 0; i < nodes.size(); i++) {
                        const Node* n = nodes[i];
                        cout << n->id << "\t" << (int) Now << "\t"  << n->Ki[day] << "\t" 
                                              << n->state_counts[SUSCEPTIBLE] << "\t" 
                                              << n->state_counts[EXPOSED] << "\t" 
                                              << n->state_counts[ASYMPTOMATIC] + n->state_counts[PRESYMPTOMATIC]<< "\t" 
                                              << n->state_counts[SYMPTOMATIC_MILD] + n->state_counts[SYMPTOMATIC_SEVERE] << "\t" 
                                              << n->state_counts[HOSPITALIZED] + n->state_counts[HOSPITALIZED_CRIT]<< "\t"
                                              << n->state_counts[CRITICAL] << "\t"
                                              << n->state_counts[DEATH] << "\t"
                                              << n->state_counts[RESISTANT] << "\t"
                                              << n->introduced << endl;
                                              //<< n->cumu_symptomatic << endl;
                    }
                    day = (int) Now;
                }

                continue;
            }
        }

        //Epidemic size not used for now
        //int current_epidemic_size() {
        //    return state_counts[EXPOSED] + state_counts[ASYMPTOMATIC] + state_counts[PRESYMPTOMATIC] + 
        //           state_counts[SYMPTOMATIC_MILD] + state_counts[SYMPTOMATIC_SEVERE] +
        //           state_counts[HOSPITALIZED] + state_counts[CRITICAL];
        //}

        void reset() {
            Now = 0.0;
            for (size_t i = 0; i < nodes.size(); i++) {
                nodes[i]->reset();
            }
            EventQ = priority_queue<Event, vector<Event>, compTime > ();
        }

        void rand_infect(int k, Node* n) {   // randomly infect k people
            for (unsigned int i = 0; i < k; i++) {
                infect(n);
            }
            return;
        }
        
        double get_detection_modifier(double p, double r) {return p * r + (1.0 - p);}
        size_t get_infection_node_id(size_t nid) {
            double total_weight = 0.0;
            size_t chosen;
            for (size_t i = 0; i < infection_matrix.size(); i++) { total_weight += infection_matrix[nid][i]; }
            double r = rand_uniform(0, total_weight, &rng);
            for (size_t i = 0; i < infection_matrix.size(); i++) { 
                if (r < infection_matrix[nid][i]) {
                    chosen = i;
                    break;
                } else {
                    r -= infection_matrix[nid][i]; 
                }
            }

            return chosen;
        }

        void infect(Node* n) {
            assert(n->state_counts[SUSCEPTIBLE] > 0);
            n->state_counts[SUSCEPTIBLE]--;  // decrement susceptible groupjj
            n->state_counts[EXPOSED]++;      // increment exposed group

            // time to become infectious (duel between presymp and asymp)
            double Tpres = rand_exp(n->Kpres, &rng) + Now;
            double Tasym = rand_exp(n->Kasym, &rng) + Now;
            double Ti;
            double Tsym;
            double Tr;
            double Th = 0;
            double Tcr = -1;
            double Thc = 0;
            double Td = 0;
            vector<double> Times;
            vector<double> Ki_modifier;

            // Start differentiating the two paths
            if (Tpres < Tasym) {
                // SYMPTOMATIC PATH
                // time to become infectious
                Ti = Tpres;
                add_event(Tpres, PRE, n, n);
                Times.push_back(Ti);
                Ki_modifier.push_back(get_detection_modifier( n->get_Pdet((int) Ti, 1), n->frac_infectiousness_det ));

                double Tmild = rand_exp(n->Kmild, &rng) + Tpres;
                double Tsevere = rand_exp(n->Ksevere, &rng) + Tpres;

                if (Tmild < Tsevere) {
                    // Mild SYMPTOMATIC PATH
                    Tsym = Tmild;
                    add_event(Tsym, SYMM, n, n);
                    Times.push_back(Tsym);
                    Ki_modifier.push_back(get_detection_modifier( 1 - (1 - n->get_Pdet((int) Ti, 1)) * (1 - n->get_Pdet((int) Tsym, 2)), n->frac_infectiousness_det ));

                    Tr = rand_exp(n->get_Krec((int) Tsym, 1), &rng) + Tsym;
                    add_event(Tr, RECM, n, n);
                } else {
                    // Severe SYMPTOMATIC PATH
                    Tsym = Tsevere;
                    add_event(Tsym, SYMS, n, n);
                    Times.push_back(Tsym);
                    Ki_modifier.push_back(get_detection_modifier( 1 - (1 - n->get_Pdet((int) Ti, 1)) * (1 - n->get_Pdet((int) Tsym, 3)), n->frac_infectiousness_det ));

                    Th = rand_exp(n->Khosp, &rng) + Tsym;
                    add_event(Th, HOS, n, n);
                    Times.push_back(Th);
                    Ki_modifier.push_back(get_detection_modifier( 1 - (1 - n->get_Pdet((int) Ti, 1)) * (1 - n->get_Pdet((int) Th, 4)) , 0 ));

                    if (rand_uniform(0, 1, &rng) > n->get_Pcrit((int) Th)) {
                        // Hospitalized and recovered
                        Tr = rand_exp(n->get_Krec((int) Th, 2), &rng) + Th;
                        add_event(Tr, RECH, n, n);
                    } else {
                        // Hospitalized and become critical
                        Tcr = rand_exp(n->Kcrit, &rng) + Th;
                        add_event(Tcr, CRI, n, n);

                        if (rand_uniform(0, 1, &rng) > n->get_Pdeath((int) Tcr)) {
                            // Critical and recovered
                            Thc = rand_exp(n->get_Krec((int) Tcr, 3), &rng) + Tcr;
                            add_event(Thc, HPC, n, n);

                            Tr = rand_exp(n->get_Krec((int) Thc, 4), &rng) + Thc;
                            add_event(Tr, RECC, n, n);
                        } else {
                            // Critical and die
                            Tr = rand_exp(n->Kdeath, &rng) + Tcr;
                            add_event(Tr, DEA, n, n);
                        } 
                    }

                }

            } else {
                // ASYMPTOMATIC PATH
                // time to become infectious
                Ti = Tasym;
                add_event(Tasym, ASY, n, n);
                Times.push_back(Ti);
                Ki_modifier.push_back(get_detection_modifier( n->get_Pdet((int) Ti, 0), n->frac_infectiousness_det ) * n->frac_infectiousness_As);

                // time to recovery
                Tr = rand_exp(n->get_Krec((int) Ti, 0), &rng) + Ti;
                add_event(Tr, RECA, n, n);
            }
            
            // time to next contact
            int bin = 0;
            double Tc = rand_exp(n->get_Ki((int) Ti) * Ki_modifier[bin], &rng) + Ti;
            while ( Tc < Tr ) {     // does contact occur before recovery?
                // decide which node to infect
                size_t infect_node_id = get_infection_node_id(n->id);
                add_event(Tc, CON, n, nodes[infect_node_id]); // potential transmission event
                while (bin < Times.size() - 1 and Times[bin+1] < Tc) {bin++;} // update bin if necessary
                Tc += rand_exp(n->get_Ki((int) Tc) * Ki_modifier[bin], &rng);
            }

            // time to become susceptible again (not used for now)
            //double Ts = Tr + immunity_duration; 
            //add_event(Ts, IMM);
            //return;
        }

        int next_event() {
            if ( EventQ.empty() ) return 0;
            Event event = EventQ.top(); // get the element
            EventQ.pop();               // remove from Q

            Now = event.time;           // advance time
            switch(event.type) {
                case PRE:
                    event.source_node->state_counts[EXPOSED]--;      // decrement exposed class
                    event.target_node->state_counts[PRESYMPTOMATIC]++;   // increment symptomatic class
                    break;
                case ASY:
                    event.source_node->state_counts[EXPOSED]--;      // decrement exposed class
                    event.target_node->state_counts[ASYMPTOMATIC]++;   // increment asymptomatic class
                    break;
                case SYMM:
                    event.source_node->state_counts[PRESYMPTOMATIC]--;      // decrement exposed class
                    event.target_node->state_counts[SYMPTOMATIC_MILD]++;   // increment symptomatic class
                    event.target_node->cumu_symptomatic++;
                    break;
                case SYMS:
                    event.source_node->state_counts[PRESYMPTOMATIC]--;      // decrement exposed class
                    event.target_node->state_counts[SYMPTOMATIC_SEVERE]++;   // increment symptomatic class
                    event.target_node->cumu_symptomatic++;
                    break;
                case HOS:
                    event.source_node->state_counts[SYMPTOMATIC_SEVERE]--;      // decrement exposed class
                    event.target_node->state_counts[HOSPITALIZED]++;   // increment symptomatic class
                    break;
                case CRI:
                    event.source_node->state_counts[HOSPITALIZED]--;      // decrement exposed class
                    event.target_node->state_counts[CRITICAL]++;   // increment symptomatic class
                    break;
                case HPC:
                    event.source_node->state_counts[CRITICAL]--;      // decrement exposed class
                    event.target_node->state_counts[HOSPITALIZED_CRIT]++;   // increment symptomatic class
                    break;
                case DEA:
                    event.source_node->state_counts[CRITICAL]--;      // decrement exposed class
                    event.target_node->state_counts[DEATH]++;   // increment symptomatic class
                    break;
                case RECA:
                    event.source_node->state_counts[ASYMPTOMATIC]--;      // decrement asymptomatic class
                    event.target_node->state_counts[RESISTANT]++;   // increment recovery class
                    break;
                case RECM:
                    event.source_node->state_counts[SYMPTOMATIC_MILD]--;      // decrement symptomatic class
                    event.target_node->state_counts[RESISTANT]++;   // increment recovery class
                    break;
                case RECH:
                    event.source_node->state_counts[HOSPITALIZED]--;      // decrement symptomatic class
                    event.target_node->state_counts[RESISTANT]++;   // increment recovery class
                    break;
                case RECC:
                    event.source_node->state_counts[HOSPITALIZED_CRIT]--;      // decrement symptomatic class
                    event.target_node->state_counts[RESISTANT]++;   // increment recovery class
                    break;
                case IMM:
                    event.source_node->state_counts[RESISTANT]--;      // decrement recovery class
                    event.target_node->state_counts[SUSCEPTIBLE]++;   // increment susceptible class
                    break;
                case CON:
                    {
                        const int rand_contact = rand_uniform_int(0, event.target_node->N, &rng);
                        if (rand_contact < event.target_node->state_counts[SUSCEPTIBLE]) {
                            if (not event.target_node->id == event.source_node->id) event.target_node->introduced++;
                            infect(event.target_node);
                        }
                    }
                    break;
                default:    
                    cerr << "Unknown event type encountered in simulator: " << event.type << "\nQuitting.\n";
            }
            return 1;
        }

        void add_event( double time, eventType type, Node* sn, Node* tn) {
            EventQ.push( Event(time,type,sn,tn) );
            return;
        }

};
#endif
