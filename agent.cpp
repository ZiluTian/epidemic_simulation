#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>

#include "agent.hpp"

/*
 * Author: Zilu Tian 
 * Date: March 30 
*/

using namespace std; 

class Record : public Log{
  public: 
    map<enum SEIHCRD, timestamp> record; 

    Record(){
      record = {
        {SUSCEPTIBLE, -1}, 
        {EXPOSED, -1}, 
        {INFECTIOUS, -1}, 
        {HOSPITALIZED, -1}, 
        {CRITICAL, -1}, 
        {RECOVERED, -1}, 
        {DECEASED, -1}
      }; 
    }

    timestamp get(enum SEIHCRD state){
      return record.find(state)->second; 
    }

    void set(enum SEIHCRD state, timestamp ts){
      record.find(state)->second = ts; 
    }

    bool timeToTransit(enum SEIHCRD state, timestamp current_time, timestamp duration){
      return (current_time - get(state))== duration; 
    }

    void printRecord(){
      printLog(record); 
    }
}; 

class LocationSummary : public Log{
  private: 
    Summary last_summary;  
    PopulationSize population = 0; 

    void updateHistory(){
      for (auto &h: daily_history){
        h.second.push_back(get(h.first)); 
      }
    }

    void updatePercentile(){
      if (population==0){ 
        population = accumulate(last_summary.begin(), last_summary.end(), 0, 
                                [](const PopulationSize prev, const pair<PopulationSize, double>& e) {
                                  return prev + e.second; 
                                }); 
      } 
      auto it = percentile_summary.begin(); 

      for (auto r: last_summary){
        it->second = 1.0*(r.second) / population; 
        ++it; 
      }
    }

    void reinitialize(){
      last_summary = daily_summary; 
      for (auto &s: daily_summary){
        s.second = 0; 
      }
    }

  public: 
    Summary daily_summary; 
    History daily_history; 
    PercentileSummary percentile_summary; 

    LocationSummary() {
      daily_summary = {
        {SUSCEPTIBLE, 0}, 
        {EXPOSED, 0}, 
        {INFECTIOUS, 0}, 
        {HOSPITALIZED, 0}, 
        {CRITICAL, 0}, 
        {RECOVERED, 0}, 
        {DECEASED, 0}
      };  

      daily_history = {
        {SUSCEPTIBLE, vector<PopulationSize>{}}, 
        {EXPOSED, vector<PopulationSize>{}}, 
        {INFECTIOUS, vector<PopulationSize>{}}, 
        {HOSPITALIZED, vector<PopulationSize>{}}, 
        {CRITICAL, vector<PopulationSize>{}}, 
        {RECOVERED, vector<PopulationSize>{}}, 
        {DECEASED, vector<PopulationSize>{}}
      }; 

      percentile_summary = {
        {SUSCEPTIBLE, 0}, 
        {EXPOSED, 0}, 
        {INFECTIOUS, 0}, 
        {HOSPITALIZED, 0}, 
        {CRITICAL, 0}, 
        {RECOVERED, 0}, 
        {DECEASED, 0}
      }; 
    }

    void inc(enum SEIHCRD state){
      daily_summary.find(state)->second += 1; 
    }

    void publish(){
      // updatePercentile(); 
      // updateHistory(); 
      reinitialize(); 
    }    

    PopulationSize get(enum SEIHCRD state){
      return daily_summary.find(state)->second; 
    }

    Summary getDaily(){
      return last_summary; 
    }

    History getHistory(){
      return daily_history; 
    }

    PercentileSummary getPercentile(){
      return percentile_summary; 
    }

    void printHistory(){
      printLog(daily_history); 
    }

    void printSummary(){
      printLog(last_summary); 
    }

    void printPercentile(){
      printLog(percentile_summary); 
    }
};  


class SEIHCRD_Transitions {
  public: 
    Record * record;  
    enum AtLocation location; 
    enum SEIHCRD health_status; 
    timestamp init_time; 

    SEIHCRD_Transitions(enum AtLocation loc, enum SEIHCRD health, timestamp ts){
      record = new Record(); 
      location = loc; 
      health_status = health; 
      init_time = ts; 
      record->set(health_status, ts); 
    }

    void S2E(timestamp ts){
      health_status = EXPOSED; 
      record -> set(health_status, ts); 
    }

    void E2I(timestamp ts){
      health_status = INFECTIOUS;
      record -> set(health_status, ts); 
    }

    void I2R(timestamp ts){
      health_status = RECOVERED;
      location = HOME; 
      record -> set(health_status, ts); 
    } 

    void I2H(timestamp ts){
      health_status = HOSPITALIZED;
      location = HOSPITAL; 
      record -> set(health_status, ts); 
    }

    void I2D(timestamp ts){
      health_status = DECEASED;
      location = CEMENTRY; 
      record -> set(health_status, ts); 
    }

    void H2C(timestamp ts){
      health_status = CRITICAL;
      location = HOSPITAL; 
      record -> set(health_status, ts); 
    }

    void H2R(timestamp ts){
      health_status = RECOVERED;
      location = HOME; 
      record -> set(health_status, ts); 
    }

    void H2D(timestamp ts){
      health_status = DECEASED;
      location = CEMENTRY; 
      record -> set(health_status, ts); 
    }

    void C2D(timestamp ts){
      health_status = DECEASED;
      location = CEMENTRY; 
      record -> set(health_status, ts); 
    }

    void C2R(timestamp ts){
      health_status = RECOVERED;
      location = HOME; 
      record -> set(health_status, ts); 
    }
}; 

class Person {
  private: 
    // non-critical death 
    bool isFatal(){
      return prob2Bool(rateByAge(FATALITY, age)); 
    }    
    
    bool isHospitalized(){
      return prob2Bool(rateByAge(HOSPITALIZATION, age)); 
    }

    bool isCritical(){
      return prob2Bool(rateByAge(ICU, age)); 
    }

    // Alternatively, define with approximation 
    // bool isCritical(){
    //   return prob2Bool(HOSPITALIZATION_CRITICAL); 
    // }

  public: 
    int age; 
    int latent_period; 
    SEIHCRD_Transitions* state; 

    bool symptomatic; 
    bool isolate; 

    Person(SEIHCRD_Transitions* initial_state){
      state = initial_state; 
      age = getAge(age_by_location.find(initial_state->location)->second); 
      symptomatic = prob2Bool(PROB_SYMPTOMATIC); 
      if (symptomatic){
        latent_period = SYMPTOMATIC_LATENT_PERIOD; 
        isolate = prob2Bool(INFECTIOUS_SELF_ISOLATE_RATIO); 
      } else {
        latent_period = ASYMPTOMATIC_LATENT_PERIOD; 
      }
    }

    Person(SEIHCRD_Transitions* initial_state, int a){
      state = initial_state; 
      age = a;  
      symptomatic = prob2Bool(PROB_SYMPTOMATIC); 
      if (symptomatic){
        latent_period = SYMPTOMATIC_LATENT_PERIOD; 
        isolate = prob2Bool(INFECTIOUS_SELF_ISOLATE_RATIO); 
      } else {
        latent_period = ASYMPTOMATIC_LATENT_PERIOD; 
      }
    }

    void personalInfo(timestamp ts){
      state->record->printRecord(); 
      cout << "Time " << ts << " Status: " << SEIHCRD[state->health_status] << " Location " << AtLocation[state->location] << " (was) Symptomatic? " << symptomatic << endl; 
    }

    // For DEV    
    // Person(enum AtLocation loc, enum SEIHCRD health, int input_age, int period, bool symp, bool iso){
    //   location = loc; 
    //   health_status = health; 
    //   age = input_age; 
    //   latent_period = period; 
    //   symptomatic = symp; 
    //   isolate = iso; 
    // }

    /*
      asymptomatic and infectious -> gamma 
      asymptomatic and exposed -> 0 
      sympotomatic and infectious -> 1.5*gamma 
      symptomatic and exposed and greater than latent period -> gamma
      symptomatic and exposed and less than latent period -> 0 
    */
    double getInfectiousness(timestamp ts) {
      if (state->health_status == SUSCEPTIBLE || state->health_status == RECOVERED || state->health_status == DECEASED) {
        return 0; 
      }

      double rand_gamma = randGamma();
      if (state->health_status == EXPOSED){
        if (symptomatic && (state->record->get(state->health_status) > latent_period)){
          return rand_gamma; 
        }
        return 0; 
      }

      if (symptomatic){
        return SYMPTOMATIC_INFECTIOUSNESS_SCALE * rand_gamma; 
      } else {
        return rand_gamma; 
      }
    }

    bool underExposed(double infectiousness, double transmission_prob, timestamp ts){
      if (prob2Bool(infectiousness * transmission_prob)){
        state->S2E(ts); 
        return true; 
      }
      return false; 
    }
    
    // handle the state transition 
    enum SEIHCRD statusUpdate(timestamp ts){
      // personalInfo(ts); 
      switch (state->health_status) {
        case EXPOSED:  
          if (symptomatic && state->record->timeToTransit(state->health_status, ts, INCUBATION_PERIOD)){
            state->E2I(ts); 
          } 
          if (!symptomatic && state->record->timeToTransit(state->health_status, ts, latent_period)){
            state->E2I(ts); 
          }
          break; 
        case INFECTIOUS: 
          if (!symptomatic && state->record->timeToTransit(state->health_status, ts, ASYMPTOMATIC_RECOVER)){
            if (isFatal()){
              state->I2D(ts); 
            } else {
              state->I2R(ts); 
            }
          } 
          if(symptomatic && state->record->timeToTransit(state->health_status, ts, HOSPITALIZATION_DELAY_MEAN)){
            if (isHospitalized()){
              state->I2H(ts); 
            }
          } 
          if(symptomatic && state->record->timeToTransit(state->health_status, ts, MILD_RECOVER)){
            state->I2R(ts); 
          }
          break;  
        case HOSPITALIZED: 
          assert(DECIDE_CRITICAL < HOSPITAL_DAYS); 
          // TODO can also use rateByAge for determining critical rate
          if (state->record->timeToTransit(state->health_status, ts, DECIDE_CRITICAL)){
            if (isCritical()){
              state->H2C(ts); 
            }
          } 
          if (state->record->timeToTransit(state->health_status, ts, HOSPITAL_DAYS)){
            if (isFatal()){
              state->H2D(ts); 
            } else {
              state->H2R(ts);
            } 
          }
          break; 
        case CRITICAL:  // roll the die only once 
          if(state->record->timeToTransit(state->health_status, ts, ICU_DAYS)){
            if (prob2Bool(CRITICAL_DEATH)){
              state->C2D(ts); 
            } else {
              state->C2R(ts); 
            }
          }
          break; 
        case SUSCEPTIBLE: 
        case RECOVERED:
        case DECEASED: 
          break;   
      }
      return state->health_status; 
    }
}; 

class Location {
  private: 
    vector<Person> population; 
    PopulationSize total; 

  public:   
    PopulationSize initial_susceptible; 
    PopulationSize initial_seed; 
    enum AtLocation location; 
    MixedAge age_description; 
    double transmission_prob; 
    LocationSummary* summary = new LocationSummary();  

    Location(enum AtLocation loc){
      initial_susceptible = population_by_location.find(loc)->second;  
      initial_seed = seed_by_location.find(loc)->second; 
      total = initial_susceptible + initial_seed; 
      location = loc; 
      age_description = MixedAge{make_pair(1, age_by_location.find(loc)->second)}; 
      transmission_prob = (TransmissionProb()).getTransProb(loc); 
    }

    Location(enum AtLocation loc, PopulationSize p, PopulationSize s, MixedAge defined_age, NPI policy) {
      initial_susceptible = p; 
      initial_seed = s; 
      total = initial_susceptible + initial_seed; 
      location = loc; 
      age_description = defined_age; 
      transmission_prob = (TransmissionProb(policy)).getTransProb(loc); 
    }

    Location(enum AtLocation loc, vector<Person> pop, MixedAge defined_age, NPI policy){
      initial_susceptible = pop.size(); 
      initial_seed = 0; 
      total = initial_susceptible + initial_seed; 
      location = loc; 
      population = pop; 
      age_description = defined_age; 
      transmission_prob = (TransmissionProb(policy)).getTransProb(loc); 
    }

    void contact(Person a, Person b, timestamp ts){
      double infectious_a = a.getInfectiousness(ts); 
      double infectious_b = b.getInfectiousness(ts); 

      if ((a.state->location != b.state->location) ||
          (infectious_a==0 && infectious_b==0) || 
          (infectious_a!=0 && infectious_b!=0)){
          return; 
      }
      // asymptomatic case at EXPOSED state is also not infectious
      if (a.state->health_status == SUSCEPTIBLE){
        a.underExposed(infectious_b, transmission_prob, ts);
      } 
      if (b.state->health_status == SUSCEPTIBLE){
        b.underExposed(infectious_a, transmission_prob, ts);
      }
      return; 
    } 

    // generate the population 
    void seed(timestamp ts){
      for (int i = 0; i < initial_seed; i++){
        SEIHCRD_Transitions* init_state = new SEIHCRD_Transitions(location, EXPOSED, ts); 
        Person person(init_state, randGaussianMixture(age_description)); 
        population.push_back(person); 
      }
    }

    void init(timestamp ts){
      seed(ts); 
      if (population.size()!=total){
        for (int i = 0; i < initial_susceptible; i++){
          SEIHCRD_Transitions* init_state = new SEIHCRD_Transitions(location, SUSCEPTIBLE, ts); 
          Person person(init_state, randGaussianMixture(age_description)); 
          population.push_back(person); 
        }
      }
      cout << "Total population size " << total << "\n"; 
    }

    void run(timestamp current_time){
      // int ncontacts = ceil(PER_CAPITA_CONTACTS*total/2); 
      int ncontacts = PER_CAPITA_CONTACTS; 

      if (location == SCHOOL){
        ncontacts *= 2; 
      }

      for(PopulationSize i = 0; i < total; ++i){
        PopulationSize idx1 = randUniform(0, total-1); 
        PopulationSize idx2 = randUniform(0, total-1); 

        // PopulationSize seed1 = randUniform(0, total-1); 
        // PopulationSize idx1 = randGaussian(seed1, ncontacts); 
        // PopulationSize idx2 = randGaussian(seed1, ncontacts); 
        // if (idx1 == idx2) {
        //   idx2 += randUniform(0, ncontacts); 
        // }
        idx1 = min(total-1, idx1); 
        idx2 = min(total-1, idx2); 
        contact(population[idx1], population[idx2], current_time);
      }
      for (auto &p: population){
        summary->inc(p.statusUpdate(current_time)); 
      }
      summary->publish(); 
    }; 

    // assume simulation always starts from 0. 
    void report(){
      // cout << "Location "<< AtLocation[location] << endl; 
      summary->printSummary(); 
      // summary->printPercentile(); 
    }

    History detailedReport(){
      cout << "Report summary for Location "<< AtLocation[location] << endl; 
      // summary->printHistory(); 
      return summary->getHistory(); 
    }
}; 

class Simulation {
  public: 
    timestamp start_time; 
    timestamp end_time; 
    int step_size; 
    int report_interval; 

    Simulation(){
      start_time = 0; 
      end_time = 100;  
      step_size = 1; 
      report_interval = 10; 
    }

    Simulation(timestamp start, timestamp end, int step, int interval){
      start_time = start; 
      end_time = end; 
      step_size = step; 
      report_interval = interval; 
    }

    void start(vector<Location> locations){
      auto checkPoint = [locations](long long int ts){
        for (auto loc: locations){
          // cout << "Report for time " <<  ts << endl; 
          cout << ts << " "; 
          loc.report();
          // loc.detailedReport();
          // cout << "\n";
        }
      }; 

      for (auto &loc: locations){
        loc.init(start_time); 
      }

      for (int timer = start_time; timer < end_time; timer += step_size) {
        for (auto &loc: locations){
          loc.run(timer);  
        }
        if (timer % report_interval == 0){
          checkPoint(timer); 
        }
      }

      checkPoint(end_time); 
    }
}; 

int main(){
  // testPerson(); 
  testSimulation(); 
  // testInfectiousness(); 
  // testPolicy(); 
  return 0; 
}

void testPerson(){
  SEIHCRD_Transitions* state1 = new SEIHCRD_Transitions(HOME, SUSCEPTIBLE, 1); 
  SEIHCRD_Transitions* state2 = new SEIHCRD_Transitions(HOME, INFECTIOUS, 1); 
  Person person1(state1);
  Person person2(state2); 
  NPI no_intervention; 
  Location * home = new Location(HOME, vector<Person>{person1, person2}, MixedAge{make_pair(1, AgeInfo{62, 5})}, no_intervention); 
  
  for (int i = 0; i < 10; ++i){
    home->contact(person1, person2, i); 
    person1.personalInfo(i); 
    person2.personalInfo(i); 
  }
}

void testSimulation(){
  Simulation sim1(0, 700, 1, 10); 

  double rate_under_20 = 0.21; 
  double rate_under_40 = 0.29; 
  double rate_under_60 = 0.27; 
  double rate_above_60 = 0.20;

  AgeInfo under_20 = AgeInfo(10, 10);    
  AgeInfo under_40 = AgeInfo(30, 10);    
  AgeInfo under_60 = AgeInfo(50, 10);    
  AgeInfo above_60 = AgeInfo(70, 10);    

  NPI no_intervention; 
  Location overall(RANDOM, 66000, 10, MixedAge{make_pair(rate_under_20, under_20), 
    make_pair(rate_under_40, under_40), make_pair(rate_under_60, under_60), 
    make_pair(rate_above_60, above_60)}, no_intervention); 


  // Location workplace(WORK, 16500, 100, MixedAge{make_pair(1, AgeInfo{30, 10})}); 
  // Location school(SCHOOL, 16500, 100, MixedAge{make_pair(0.2, AgeInfo{20, 5})}); 
  // Location home(HOME, 16500, 100, MixedAge{make_pair(1, AgeInfo{40, 10})}); 
  // Location nursing_home(RANDOM, 16500, 100, MixedAge{make_pair(1, AgeInfo{70, 8})}); 

  sim1.start(vector<Location>{overall}); 
}

void testInfectiousness(){
  int trials = 10000; 
  double total = 0; 
  
  for (int i = 0; i < trials; i++){
    total += randGamma(); 
  }
  // mean is 1
  assert(abs(total/trials) - 1 < 0.1); 
  cout << "Tests for infectiousness passed "<< endl; 
}

void testPolicy(){
  NPI CI(0, 0.75, 0.75, 0.75, 0.7); 
  NPI no_intervention; 

  TransmissionProb* initial_config = new TransmissionProb(no_intervention); 
  assert(initial_config->getTransProb(HOME) == 0.33); 
  assert(initial_config->getTransProb(WORK) == 0.17); 
  
  // compliance 1: <0.66, 0.08, 0.08, 0.16>  
  // compliance 0.7: <0.56, 0.11, 0.11, 0.21>

  TransmissionProb case_isolation(CI); 
  assert(abs(case_isolation.getTransProb(HOME) - 0.56) < 0.05); 
  assert(abs(case_isolation.getTransProb(WORK) - 0.11) < 0.05); 

  cout << "Tests for TransmissionProb passed\n"; 
}
