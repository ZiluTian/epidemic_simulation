#include <map>
#include <random>
#include <iostream>
#include <cmath>
#include <utility>
#include <string> 

/*
 * Author: Zilu Tian 
 * Date: March 30 
*/

using namespace std; 

// states definition
enum SEIHCRD {SUSCEPTIBLE, EXPOSED, INFECTIOUS, HOSPITALIZED, CRITICAL, RECOVERED, DECEASED}; 
enum AtLocation {HOME, SCHOOL, WORK, RANDOM, HOSPITAL, CEMENTRY};  
enum RateCategory {HOSPITALIZATION, ICU, FATALITY}; 

string SEIHCRD[] = {
  "SUSCEPTIBLE", "EXPOSED", "INFECTIOUS", "HOSPITALIZED", "CRITICAL", "RECOVERED", "DECEASED"
}; 

string AtLocation[] = {
  "HOME", "SCHOOL", "WORK", "RANDOM", "HOSPITAL", "CEMENTRY"
}; 

// 0.1 day as simulation unit 
// TODO: DEBUG, use day 1 
// #define DAY 1
// #define INCUBATION_PERIOD 2*DAY 
// #define SYMPTOMATIC_LATENT_PERIOD 3*DAY
// #define ASYMPTOMATIC_LATENT_PERIOD 3*DAY

#define DAY 10
#define SIMULATION_TIME_UNIT DAY 
#define PROB_SYMPTOMATIC 0.67
#define MILLION 1000000

#define INCUBATION_PERIOD 51 // 5.1 days 
#define SYMPTOMATIC_LATENT_PERIOD 46
#define ASYMPTOMATIC_LATENT_PERIOD 70

#define INFECTIOUS_ALPHA 0.25
#define INFECTIOUS_BETA 4
 
#define HOSPITALIZATION_DELAY_MEAN 5*DAY
#define INFECTIOUS_SELF_ISOLATE_RATIO 2/3
#define HOSPITALIZATION_CRITICAL 0.3 
#define CRITICAL_DEATH 0.5 

#define HOSPITAL_DAYS 8*DAY
#define CRITICAL_DAYS 16*DAY 
#define DECIDE_CRITICAL 6*DAY  // no greater than HOSPITAL_DAYS. asserted in code.  
#define ICU_DAYS 10*DAY 
#define ASYMPTOMATIC_RECOVER 21*DAY
#define MILD_RECOVER 14*DAY

#define SYMPTOMATIC_INFECTIOUSNESS_SCALE 1.5 

#define PER_CAPITA_CONTACTS 12

typedef long long int timestamp; 
typedef long long int PopulationSize; 

typedef pair<double, double> AgeInfo; // mean, variance 
typedef vector<pair<double, AgeInfo>> MixedAge; // mixed gaussian   
typedef map<enum SEIHCRD, PopulationSize> Summary; 
typedef map<enum SEIHCRD, double> PercentileSummary; 

mt19937 generator(random_device{}()); 

bool prob2Bool(double, double precision = 0.001); 
int getAge(enum AtLocation location); 

int randGaussian(double mean, double var);  
double randGamma(double a = INFECTIOUS_ALPHA, double b = INFECTIOUS_BETA); 
int randUniform(int l, int u); 
int randGaussianMixture(vector<pair<double, pair<double, double>>> mixture_spec);  
// int randGaussianMixture(vector<pair<double, AgeInfo>> mixture_spec)

void testPolicy(); 
void testInfectiousness(); 
void testSimulation(); 
// void testPerson(); 

// Estimation
map<enum AtLocation, PopulationSize> population_by_location = {
  {SCHOOL, 16.5*MILLION}, 
  {HOME, 16.5*MILLION}, 
  {WORK, 16.5*MILLION}, 
  {RANDOM, 16.5*MILLION}
}; 

// TODO: UPDATE WITH PROPER ASSUMPTION
map<enum AtLocation, PopulationSize> seed_by_location = {
  {SCHOOL, 100000}, 
  {HOME, 100000}, 
  {WORK, 100000}, 
  {RANDOM, 100000}
}; 

// 21% aged under 18 
// 29% 18-39
// 27% 40-59
// 20% 60+
map<enum AtLocation, AgeInfo> age_by_location = {
  {SCHOOL, make_pair(15, 5)}, 
  {HOME, make_pair(40, 20)}, 
  {WORK, make_pair(45, 8)}, 
  {RANDOM, make_pair(60, 20)} 
}; 

int randGaussianMixture(vector<pair<double, pair<double, double>>> mixture_spec){
  while (true) {
    double guard_check = 0; 
    for (auto e: mixture_spec){
      guard_check += e.first; 
      if(prob2Bool(e.first)){
        return randGaussian(e.second.first, e.second.second); 
      }
    }
    if (abs(guard_check - 1) > 0.2){
      throw "Invalid mixed ratio!"; 
    }
    // in the unlikely event that all missed, try again. 
  }
}

int randGaussian(double mean, double var){
  auto normDist = [](double u, double v){
    normal_distribution<double> normal_dist(u, v); 
    return normal_dist(generator); 
  }; 
  int ans = ceil(normDist(mean, var)); 
  if (ans<0){ return 0; }
  return ans; 
}

int getAge(AgeInfo ages){
  return randGaussian(ages.first, ages.second); 
}

int getHospitalizationDelay(){
  return randGaussian(HOSPITALIZATION_DELAY_MEAN, 3*DAY); 
}

double randGamma(double alpha, double beta) {
  auto getGamma = [alpha, beta]() {
    gamma_distribution<double> gamma_dist(alpha, beta); 
    return gamma_dist(generator); 
  }; 
  return getGamma(); 
}

int randUniform(int l, int u){
  uniform_int_distribution<> uniform_dist(l, u); 
  return uniform_dist(generator); 
}

map<enum AtLocation, double> initial_transmission_prob = {
  {HOME, 0.33},
  {SCHOOL, 0.17},
  {WORK, 0.17},
  {RANDOM, 0.33}
}; 

// age_group, % symptomatic cases requiring hospitalization 
map<int, double> symptomatic_hospitalization_rate = {
  {0, 0.001}, 
  {1, 0.003}, 
  {2, 0.012}, 
  {3, 0.032}, 
  {4, 0.049}, 
  {5, 0.102}, 
  {6, 0.166}, 
  {7, 0.243}, 
  {8, 0.273} 
}; 

map<int, double> hospitalized_critical_care_rate = {
  {0, 0.05}, 
  {1, 0.05}, 
  {2, 0.05}, 
  {3, 0.05}, 
  {4, 0.063}, 
  {5, 0.122}, 
  {6, 0.274}, 
  {7, 0.432}, 
  {8, 0.709} 
}; 

map<int, double> infection_fatality_rate = {
  {0, 0.00002}, 
  {1, 0.00006}, 
  {2, 0.0003}, 
  {3, 0.0008}, 
  {4, 0.0015}, 
  {5, 0.0060}, 
  {6, 0.022}, 
  {7, 0.051}, 
  {8, 0.093} 
}; 

double rateByAge(enum RateCategory rate_category, int age) {
  auto compute_age_group = [](int age) {
    if (age > 80){
      return 8; 
    } else {
      return age/10;
    }
  }; 

  int age_group = compute_age_group(age); 

  double ans = 0; 
  switch (rate_category){
    case HOSPITALIZATION: 
      ans = symptomatic_hospitalization_rate.find(age_group)->second; break; 
    case ICU: 
      ans = hospitalized_critical_care_rate.find(age_group)->second; break; 
    case FATALITY: 
      ans = infection_fatality_rate.find(age_group)->second; break;   
  }
  return ans; 
}

bool prob2Bool(double probability, double precision){
  auto double2int = [](double num){
    return static_cast<int> (num); 
  }; 
  return randUniform(0, double2int(1/precision)) < double2int(probability/precision); 
}


// Non-Pharmaceutical Intervention 
class NPI {
  public: 
    double reduced_home_contact_rate; 
    double reduced_school_contact_rate; 
    double reduced_work_contact_rate; 
    double reduced_random_contact_rate; 
    double compliance_rate; 

    NPI(){
      reduced_home_contact_rate = 0; 
      reduced_school_contact_rate = 0; 
      reduced_work_contact_rate = 0; 
      reduced_random_contact_rate = 0; 
      compliance_rate = 1; 
    }

    NPI(double home, double school, double work, double random, double compliance){
      reduced_home_contact_rate = home; 
      reduced_school_contact_rate = school; 
      reduced_work_contact_rate = work; 
      reduced_random_contact_rate = random; 
      compliance_rate = compliance; 
    } 
}; 

class TransmissionProb {
  private: 
    double getInitProb (enum AtLocation loc) {
      return initial_transmission_prob.find(loc) -> second; 
    } 

    map<enum AtLocation, double> evaluatePolicy(NPI policy){
      vector<double> originals {getInitProb(HOME), getInitProb(SCHOOL), getInitProb(WORK), getInitProb(RANDOM)}; 

      auto applyReduction = [originals, policy](){
        vector<double> reductions {policy.reduced_home_contact_rate, 
          policy.reduced_school_contact_rate, 
          policy.reduced_work_contact_rate, 
          policy.reduced_random_contact_rate}; 
        vector<double> intermediate_res; 
        vector<double> ans; 

        auto it = originals.begin(); 
        for (auto j: reductions){
          intermediate_res.push_back((*it) * (1 - j)); 
          it++; 
        }

        double sum = accumulate(intermediate_res.begin(), intermediate_res.end(), 0.0);
        for (auto j: intermediate_res){
          ans.push_back(j/sum); 
        }
        return ans; 
      }; 

      auto applyCompliance = [policy, originals, applyReduction](){
        vector<double> apply_reduction = applyReduction(); 
        
        vector<enum AtLocation> locations {HOME, SCHOOL, WORK, RANDOM}; 
        auto red_it = apply_reduction.begin(); 
        auto orig_it = originals.begin(); 
        map<enum AtLocation, double> ans{}; 

        for (auto loc: locations){
          ans.insert(pair<enum AtLocation, double>
            (loc, ((1-policy.compliance_rate) * (*orig_it) + (policy.compliance_rate * (*red_it))))); 
          ++red_it;  
          ++orig_it; 
        }
        return ans; 
      }; 

      map<enum AtLocation, double> ans{}; 
      vector<enum AtLocation> locations {HOME, SCHOOL, WORK, RANDOM}; 
      auto red_it = applyReduction().begin(); 

      for (auto loc: locations){
        ans.insert(pair<enum AtLocation, double>(loc, *red_it)); 
        ++red_it; 
      }

      if (abs(policy.compliance_rate - 1) > 0.01) { return applyCompliance(); }
      return ans;  
    }

  public: 
    map<enum AtLocation, double> transmission_map; 

    TransmissionProb(){
      transmission_map = initial_transmission_prob; 
    }

    TransmissionProb(NPI policy){
      transmission_map = evaluatePolicy(policy); 
    }

    double getTransProb (enum AtLocation loc) {
      return transmission_map.find(loc) -> second; 
    } 
}; 

// TODO: consider using template
class Log {
  private: 
    map<enum SEIHCRD, long long int> log_template = {
      {SUSCEPTIBLE, 0}, 
      {EXPOSED, 0}, 
      {INFECTIOUS, 0}, 
      {HOSPITALIZED, 0}, 
      {CRITICAL, 0}, 
      {RECOVERED, 0}, 
      {DECEASED, 0}
    }; 

    map<enum SEIHCRD, double> percentile_template = {
      {SUSCEPTIBLE, 0}, 
      {EXPOSED, 0}, 
      {INFECTIOUS, 0}, 
      {HOSPITALIZED, 0}, 
      {CRITICAL, 0}, 
      {RECOVERED, 0}, 
      {DECEASED, 0}
    }; 

    void viewAsPercentile(Summary summary){
      map<enum SEIHCRD, double> percentile_summary = percentile_template; 
      int population = accumulate(summary.begin(), summary.end(), 0, 
                              [](const PopulationSize prev, const pair<PopulationSize, double>& e) {
                                return prev + e.second; 
                              }); 
      auto it = percentile_summary.begin(); 
      for (auto r: summary){
        it->second = 1.0*(r.second) / population; 
        ++it; 
      }

      for (auto e: percentile_summary){
        cout << e.second << " "; 
      }
      cout << endl; 
    }

  public:  

    map<enum SEIHCRD, long long int> log; 

    Log(){
      log = log_template; 
    }

    map<enum SEIHCRD, long long int> logTemplate(int init_val){
      map<enum SEIHCRD, long long int> ans = log_template; 
      for (auto e: ans){
        e.second = init_val; 
      }
      return ans; 
    }

    Summary aggregateSummary(vector<Summary> regional_summary){
      Summary aggregate_summary = log_template; 

      for (auto regional_map: regional_summary){
        for (auto s: regional_map){
          aggregate_summary.find(s.first)->second += s.second; 
        }
      }
      return aggregate_summary;    
    } 

    void printLog(){  
      for (auto e: log){
        cout << e.second << " "; 
      }
      cout << endl; 
    }

    void printPercent(){
      viewAsPercentile(log); 
    }

    void printPercent(Summary s){
      viewAsPercentile(s); 
    }
}; 