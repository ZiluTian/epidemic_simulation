#include <vector>
#include <algorithm>
#include <numeric>

#include "agent.hpp"

/*
 * Author: Zilu Tian 
 * Date: March 30 
*/

using namespace std; 

class Record : public Log{
  public: 
    map<enum SEIHCRD, timestamp> record = {
      {SUSCEPTIBLE, -1}, 
      {EXPOSED, -1}, 
      {INFECTIOUS, -1}, 
      {HOSPITALIZED, -1}, 
      {CRITICAL, -1}, 
      {RECOVERED, -1}, 
      {DECEASED, -1}
    }; 

    Record(){}

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
    Summary daily_summary = {
      {SUSCEPTIBLE, 0}, 
      {EXPOSED, 0}, 
      {INFECTIOUS, 0}, 
      {HOSPITALIZED, 0}, 
      {CRITICAL, 0}, 
      {RECOVERED, 0}, 
      {DECEASED, 0}
    }; 

    History daily_history = {
        {SUSCEPTIBLE, vector<PopulationSize>{}}, 
        {EXPOSED, vector<PopulationSize>{}}, 
        {INFECTIOUS, vector<PopulationSize>{}}, 
        {HOSPITALIZED, vector<PopulationSize>{}}, 
        {CRITICAL, vector<PopulationSize>{}}, 
        {RECOVERED, vector<PopulationSize>{}}, 
        {DECEASED, vector<PopulationSize>{}}
    }; 

    PercentileSummary percentile_summary = {
      {SUSCEPTIBLE, 0}, 
      {EXPOSED, 0}, 
      {INFECTIOUS, 0}, 
      {HOSPITALIZED, 0}, 
      {CRITICAL, 0}, 
      {RECOVERED, 0}, 
      {DECEASED, 0}
    }; 

    LocationSummary(){}; 

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
    Record * record = new Record();  
    enum AtLocation location; 
    enum SEIHCRD health_status; 

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

class Person : public SEIHCRD_Transitions {
  private: 
    int age; 
    int latent_period; 

    bool symptomatic; 
    bool isolate; 

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

    void personalInfo(timestamp ts){
      record->printRecord(); 
      cout << "Time " << ts << " Status: " << SEIHCRD[health_status] << " Location " << AtLocation[location] << " (was) Symptomatic? " << symptomatic << endl; 
    }

  public: 
    Person(enum AtLocation loc, enum SEIHCRD health){
      age = getAge(age_by_location.find(loc)->second); 
      location = loc;  
      symptomatic = prob2Bool(PROB_SYMPTOMATIC); 
      health_status = health; 
      if (symptomatic){
        latent_period = SYMPTOMATIC_LATENT_PERIOD; 
        isolate = prob2Bool(INFECTIOUS_SELF_ISOLATE_RATIO); 
      } else {
        latent_period = ASYMPTOMATIC_LATENT_PERIOD; 
      }
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
      if (health_status == SUSCEPTIBLE || health_status == RECOVERED || health_status == DECEASED) {
        return 0; 
      }

      double rand_gamma = randGamma();
      if (health_status == EXPOSED){
        if (symptomatic && (record->get(health_status) > latent_period)){
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
        S2E(ts); 
        return true; 
      }
      return false; 
    }
    
    // handle the state transition 
    enum SEIHCRD statusUpdate(timestamp ts){
      // personalInfo(ts); 
      switch (health_status) {
        case EXPOSED:  
          if (symptomatic && record->timeToTransit(health_status, ts, INCUBATION_PERIOD)){
            E2I(ts); 
          } 
          if (!symptomatic && record->timeToTransit(health_status, ts, latent_period)){
            E2I(ts); 
          }
          break; 
        case INFECTIOUS: 
          if (!symptomatic && record->timeToTransit(health_status, ts, ASYMPTOMATIC_RECOVER)){
            if (isFatal()){
              I2D(ts); 
            } else {
              I2R(ts); 
            }
          } 
          if(symptomatic && record->timeToTransit(health_status, ts, HOSPITALIZATION_DELAY_MEAN)){
            if (isHospitalized()){
              I2H(ts); 
            }
          } 
          if(symptomatic && record->timeToTransit(health_status, ts, MILD_RECOVER)){
            I2R(ts); 
          }
          break;  
        case HOSPITALIZED: 
          assert(DECIDE_CRITICAL < HOSPITAL_DAYS); 
          // TODO can also use rateByAge for determining critical rate
          if (record->timeToTransit(health_status, ts, DECIDE_CRITICAL)){
            if (isCritical()){
              H2C(ts); 
            }
          } 
          if (record->timeToTransit(health_status, ts, HOSPITAL_DAYS)){
            if (isFatal()){
              H2D(ts); 
            } else {
              H2R(ts);
            } 
          }
          break; 
        case CRITICAL:  // roll the die only once 
          if(record->timeToTransit(health_status, ts, ICU_DAYS)){
            if (prob2Bool(CRITICAL_DEATH)){
              C2D(ts); 
            } else {
              C2R(ts); 
            }
          }
          break; 
        case SUSCEPTIBLE: 
        case RECOVERED:
        case DECEASED: 
          break;   
      }
      return health_status; 
    }
}; 

class Location : TransmissionProb {
  private: 
    vector<Person> population; 
    PopulationSize total; 

  public:   
    PopulationSize initial_susceptible; 
    PopulationSize initial_seed; 
    enum AtLocation location; 
    LocationSummary* summary = new LocationSummary();  

    Location(enum AtLocation loc){
      initial_susceptible = population_by_location.find(loc)->second;  
      initial_seed = seed_by_location.find(loc)->second; 
      total = initial_susceptible + initial_seed; 
      location = loc; 
    }

    Location(enum AtLocation loc, PopulationSize p, PopulationSize s){
      initial_susceptible = p; 
      initial_seed = s; 
      total = initial_susceptible + initial_seed; 
      location = loc; 
    }

    Location(enum AtLocation loc, vector<Person> pop){
      initial_susceptible = pop.size(); 
      initial_seed = 0; 
      total = initial_susceptible + initial_seed; 
      location = loc; 
      population = pop; 
    }

    void contact(Person& a, Person& b, timestamp ts){
      double infectious_a = a.getInfectiousness(ts); 
      double infectious_b = b.getInfectiousness(ts); 
      
      auto noRisk = [infectious_a, infectious_b, a, b](){
        return ((a.location != b.location) ||
          (infectious_a==0 && infectious_b==0) || 
          (infectious_a!=0 && infectious_b!=0)); 
      }; 

      // asymptomatic case at EXPOSED state is also not infectious
      // pointer is too messy, const person can't modify state 
      auto susceptiblePerson = [a, b]() -> int {
        if (a.health_status == SUSCEPTIBLE) { return 1; }
        if (b.health_status == SUSCEPTIBLE) { return 2; }
        return -1;  
      }; 
      
      auto infectiousness = [infectious_a, infectious_b](){
        if (infectious_a > 0) { return infectious_a; }
        if (infectious_b > 0) { return infectious_b; }
      }; 

      if (!noRisk()){
        if (susceptiblePerson()==1){
          a.underExposed(infectiousness(), getTransProb(location), ts); 
        } 
        if (susceptiblePerson()==2){
          b.underExposed(infectiousness(), getTransProb(location), ts); 
        }
      }
    } 

    // generate the population 
    void seed(timestamp ts){
      for (int i = 0; i < initial_seed; i++){
        Person person(location, EXPOSED); 
        person.record->set(EXPOSED, ts); 
        population.push_back(person); 
      }
    }

    void init(timestamp ts){
      seed(ts); 
      if (population.size()!=total){
        for (int i = 0; i < initial_susceptible; i++){
          Person person(location, SUSCEPTIBLE); 
          person.record->set(SUSCEPTIBLE, ts); 
          population.push_back(person); 
        }
      }
      cout << "Total population size " << total << "\n"; 
    }

    void run(timestamp current_time){
      int ncontacts = ceil(PER_CAPITA_CONTACTS*total/2); 
      if (location == SCHOOL){
        ncontacts *= 2; 
      }
      for(PopulationSize i = 0; i < ncontacts; ++i){
        PopulationSize idx1 = randUniform(0, total-1); 
        PopulationSize idx2 = randUniform(0, total-1); 
        contact(population[idx1], population[idx2], current_time);
      }
      for (auto &p: population){
        summary->inc(p.statusUpdate(current_time)); 
      }
      summary->publish(); 
    }; 

    // assume simulation always starts from 0. 
    void report(){
      cout << "Location "<< AtLocation[location] << endl; 
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
          cout << "Report for time " <<  ts << endl; 
          loc.report(); 
          // loc.detailedReport();
          cout << "\n";  
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
  testSimulation(); 
  // testInfectiousness(); 
  // testPolicy(); 
  return 0; 
}

void testSimulation(){
  Simulation sim1(0, 700, 1, 50); 

  Location workplace(WORK, 16500, 100); 
  Location school(SCHOOL, 16500, 100); 
  Location home(HOME, 16500, 100); 
  Location nursing_home(RANDOM, 16500, 100); 

  sim1.start(vector<Location>{home, workplace, school, nursing_home}); 
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
  TransmissionProb initial_config; 
  NPI CI(0, 0.75, 0.75, 0.75, 0.7); 
  assert(initial_config.transmission_map.find(HOME)->second == 0.33); 
  assert(initial_config.transmission_map.find(WORK)->second == 0.17); 
  initial_config.changeTransmissionProb(CI); 
  // compliance 1: <0.66, 0.08, 0.08, 0.16>  
  // compliance 0.7: <0.56, 0.11, 0.11, 0.21>
  assert(abs(initial_config.transmission_map.find(HOME)->second - 0.56) < 0.05); 
  assert(abs(initial_config.transmission_map.find(WORK)->second - 0.11) < 0.05); 
  cout << "Tests for TransmissionProb passed\n"; 
}
