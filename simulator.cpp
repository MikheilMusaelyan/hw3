#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

struct Process {
    string pid;
    int arrival;
    int burst;
    int priority;
    int order;
};

struct Segment {
    int index;
    int start;
    int end;
};

struct Result {
    vector<Segment> schedule;
    vector<int> start;
    vector<int> completion;
};

vector<Process> readWorkload(const string& filename)
{
    ifstream file(filename);
    vector<Process> processes;
    string line;
    int order = 0;

    while (getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue;

        replace(line.begin(), line.end(), ',', ' ');
        stringstream input(line);
        Process process;
        if (input >> process.pid >> process.arrival >> process.burst >> process.priority) {
            process.order = order++;
            processes.push_back(process);
        }
    }

    return processes;
}

void addSegment(Result& result, int index, int start, int end)
{
    if (start == end)
        return;
    result.schedule.push_back({index, start, end});
}

int nextProcess(const vector<Process>& processes, const vector<bool>& finished,
                int time, int choice)
{
    int selected = -1;

    for (int i = 0; i < (int)processes.size(); i++) {
        if (finished[i] || processes[i].arrival > time)
            continue;

        if (selected == -1) {
            selected = i;
            continue;
        }

        bool replace = false;
        if (choice == 0)
            replace = processes[i].order < processes[selected].order;
        else if (choice == 1)
            replace = processes[i].burst < processes[selected].burst;
        else
            replace = processes[i].priority < processes[selected].priority;

        if (replace)
            selected = i;
    }

    return selected;
}

Result nonPreemptive(const vector<Process>& processes, int choice)
{
    Result result;
    result.start.assign(processes.size(), -1);
    result.completion.assign(processes.size(), 0);
    vector<bool> finished(processes.size(), false);
    int time = 0;

    for (int done = 0; done < (int)processes.size(); done++) {
        int index = nextProcess(processes, finished, time, choice);
        if (index == -1) {
            int next = -1;
            for (int i = 0; i < (int)processes.size(); i++) {
                if (!finished[i] && (next == -1 || processes[i].arrival < next))
                    next = processes[i].arrival;
            }
            time = next;
            index = nextProcess(processes, finished, time, choice);
        }

        result.start[index] = time;
        time += processes[index].burst;
        result.completion[index] = time;
        addSegment(result, index, result.start[index], time);
        finished[index] = true;
    }

    return result;
}

Result roundRobin(const vector<Process>& processes, int quantum)
{
    Result result;
    result.start.assign(processes.size(), -1);
    result.completion.assign(processes.size(), 0);
    vector<int> remaining(processes.size());
    vector<bool> added(processes.size(), false);
    queue<int> ready;
    int time = 0;
    int done = 0;

    for (int i = 0; i < (int)processes.size(); i++)
        remaining[i] = processes[i].burst;

    while (done < (int)processes.size()) {
        for (int i = 0; i < (int)processes.size(); i++) {
            if (!added[i] && processes[i].arrival <= time) {
                ready.push(i);
                added[i] = true;
            }
        }

        if (ready.empty()) {
            for (int i = 0; i < (int)processes.size(); i++)
                if (!added[i] && (time == 0 || processes[i].arrival < time))
                    time = processes[i].arrival;
            continue;
        }

        int index = ready.front();
        ready.pop();
        if (result.start[index] == -1)
            result.start[index] = time;

        int run = min(quantum, remaining[index]);
        int start = time;
        time += run;
        remaining[index] -= run;
        addSegment(result, index, start, time);

        for (int i = 0; i < (int)processes.size(); i++) {
            if (!added[i] && processes[i].arrival <= time) {
                ready.push(i);
                added[i] = true;
            }
        }

        if (remaining[index] == 0) {
            result.completion[index] = time;
            done++;
        } else {
            ready.push(index);
        }
    }

    return result;
}

Result mlfq(const vector<Process>& processes)
{
    Result result;
    result.start.assign(processes.size(), -1);
    result.completion.assign(processes.size(), 0);
    vector<int> remaining(processes.size());
    vector<bool> added(processes.size(), false);
    queue<int> queues[3];
    int time = 0;
    int done = 0;

    for (int i = 0; i < (int)processes.size(); i++)
        remaining[i] = processes[i].burst;

    while (done < (int)processes.size()) {
        for (int i = 0; i < (int)processes.size(); i++) {
            if (!added[i] && processes[i].arrival <= time) {
                queues[0].push(i);
                added[i] = true;
            }
        }

        int level = 0;
        while (level < 3 && queues[level].empty())
            level++;
        if (level == 3) {
            int next = -1;
            for (int i = 0; i < (int)processes.size(); i++)
                if (!added[i] && (next == -1 || processes[i].arrival < next))
                    next = processes[i].arrival;
            time = next;
            continue;
        }

        int index = queues[level].front();
        queues[level].pop();
        if (result.start[index] == -1)
            result.start[index] = time;

        int quantum = level == 0 ? 4 : level == 1 ? 8 : 16;
        int run = min(quantum, remaining[index]);
        int start = time;
        time += run;
        remaining[index] -= run;
        addSegment(result, index, start, time);

        for (int i = 0; i < (int)processes.size(); i++) {
            if (!added[i] && processes[i].arrival <= time) {
                queues[0].push(i);
                added[i] = true;
            }
        }

        if (remaining[index] == 0) {
            result.completion[index] = time;
            done++;
        } else {
            queues[min(level + 1, 2)].push(index);
        }
    }

    return result;
}

void printResult(const vector<Process>& processes, const Result& result)
{
    cout << left << setw(8) << "PID" << setw(8) << "Arrival" << setw(8) << "Burst"
         << setw(8) << "Start" << setw(12) << "Completion" << setw(10) << "Waiting"
         << "Turnaround Response" << endl;

    double totalWaiting = 0;
    double totalTurnaround = 0;
    double totalResponse = 0;

    for (int i = 0; i < (int)processes.size(); i++) {
        int turnaround = result.completion[i] - processes[i].arrival;
        int waiting = turnaround - processes[i].burst;
        int response = result.start[i] - processes[i].arrival;
        totalWaiting += waiting;
        totalTurnaround += turnaround;
        totalResponse += response;

        cout << left << setw(8) << processes[i].pid << setw(8) << processes[i].arrival
             << setw(8) << processes[i].burst << setw(8) << result.start[i]
             << setw(12) << result.completion[i] << setw(10) << waiting
             << turnaround << "       " << response << endl;
    }

    cout << fixed << setprecision(2);
    cout << "Average Waiting Time: " << totalWaiting / processes.size() << endl;
    cout << "Average Turnaround Time: " << totalTurnaround / processes.size() << endl;
    cout << "Average Response Time: " << totalResponse / processes.size() << endl;
}

Result run(const vector<Process>& processes, const string& algorithm, int quantum)
{
    if (algorithm == "fcfs")
        return nonPreemptive(processes, 0);
    if (algorithm == "sjf")
        return nonPreemptive(processes, 1);
    if (algorithm == "priority")
        return nonPreemptive(processes, 2);
    if (algorithm == "rr" || algorithm == "round-robin" || algorithm == "round_robin")
        return roundRobin(processes, quantum);
    return mlfq(processes);
}

int main(int argc, char* argv[])
{
    string filename = "sample.csv";
    string algorithm = "fcfs";
    bool compare = false;
    int quantum = 4;

    for (int i = 1; i < argc; i++) {
        string argument = argv[i];
        if (argument == "--workload" && i + 1 < argc)
            filename = argv[++i];
        else if (argument == "--algo" && i + 1 < argc)
            algorithm = argv[++i];
        else if (argument == "--quantum" && i + 1 < argc)
            quantum = stoi(argv[++i]);
        else if (argument == "--compare")
            compare = true;
    }

    vector<Process> processes = readWorkload(filename);
    if (processes.empty()) {
        cerr << "No processes found in " << filename << endl;
        return 1;
    }

    if (compare) {
        vector<string> algorithms = {"fcfs", "sjf", "rr", "priority", "mlfq"};
        cout << left << setw(12) << "Algorithm" << setw(18) << "Avg Waiting"
             << setw(20) << "Avg Turnaround" << "Avg Response" << endl;
        for (const string& name : algorithms) {
            Result result = run(processes, name, quantum);
            double waiting = 0;
            double turnaround = 0;
            double response = 0;
            for (int i = 0; i < (int)processes.size(); i++) {
                turnaround += result.completion[i] - processes[i].arrival;
                waiting += result.completion[i] - processes[i].arrival - processes[i].burst;
                response += result.start[i] - processes[i].arrival;
            }
            cout << left << setw(12) << name << fixed << setprecision(2)
                 << setw(18) << waiting / processes.size() << setw(20)
                 << turnaround / processes.size() << response / processes.size() << endl;
        }
    } else {
        printResult(processes, run(processes, algorithm, quantum));
    }

    return 0;
}
