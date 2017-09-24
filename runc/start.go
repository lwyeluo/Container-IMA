package main

import (
	"errors"
	"fmt"

	"github.com/opencontainers/runc/libcontainer"

	"github.com/urfave/cli"
	"os"
	"log"

	io "io/ioutil"
	"encoding/json"
	"os/exec"
	"strings"
        "crypto/sha1"
)

func record(digest string) error {
	var filename = "/var/log/docker-boot.log"

	f, err := os.OpenFile(filename, os.O_RDWR | os.O_CREATE | os.O_APPEND, 0644)
	if err != nil {
		return err
	}

	defer f.Close()

	n, err := f.WriteString(digest + "\n")
	if err == nil && n < len(digest) {
		err = errors.New("short write")
	}

	return err
}

// @return:
//	string: the digest of image
//	string: config filename
//	err
func getDigest(id string, debugLog *log.Logger) (string, string, error) {

	// load config file which includes the information of image
	var dict map[string]interface{}
	configFileName := "/var/lib/docker/containers/" + id + "/config.v2.json"
	data, err := io.ReadFile(configFileName)
	if err != nil {
		debugLog.Fatalln("cannot find image")
		return "", "", err
	}

	datajson := []byte(data)
	err = json.Unmarshal(datajson, &dict)
	if err != nil {
		debugLog.Fatalln("failed to load config")

	}

	// get the digest of this image
	imageDigest, ok := dict["Image"].(string)
	if ok {
		debugLog.Println(">> The image digest is: " + imageDigest)
	}

	return imageDigest, configFileName, err
}

// @Args:
//	pid: the first pid of this container
//	debugLog
// @Returns:
//	the mnt_namspace number
//	err
func getMntNamespace(pid int, debugLog *log.Logger) (string, error) {
	cmd := fmt.Sprintf("ls -l /proc/%d/ns | grep mnt | awk -F \":\" '{print $3}'", pid)
	cmdExec := exec.Command("/bin/bash", "-c", cmd)
	output, err := cmdExec.Output()

	debugLog.Printf(">> The mnt namespace is: %s\n", output)

	if err != nil {
		return "", err
	}

	return strings.Replace(string(output), "\n", "", -1), nil
}

// @Args:
//	pcrIndex: the index of PCR to be extend
//	value: the value to extend
//	debugLog
// @Returns:
//	err
func extend(pcrIndex int, value string, debugLog *log.Logger) error {
	// extend the digest into TPM
	cmd := fmt.Sprintf("/usr/bin/tpm-extend -p %d -v %s", pcrIndex, value)
	cmdExec := exec.Command("/bin/bash", "-c", cmd)
	output, err := cmdExec.Output()
	//if err != nil {
	//	return err
	//}
	debugLog.Printf("%s\n", output)
	debugLog.Println(err)

	return err
}

var startCommand = cli.Command{
	Name:  "start",
	Usage: "executes the user defined process in a created container",
	ArgsUsage: `<container-id>

Where "<container-id>" is your name for the instance of the container that you
are starting. The name you provide for the container instance must be unique on
your host.`,
	Description: `The start command executes the user defined process in a created container.`,
	Action: func(context *cli.Context) error {
		if err := checkArgs(context, 1, exactArgs); err != nil {
			return err
		}
		container, err := getContainer(context)
		if err != nil {
			return err
		}

		status, err := container.Status()
		if err != nil {
			return err
		}
		switch status {
		case libcontainer.Created:
			err = container.Exec()
			return err
		case libcontainer.Stopped:
			return errors.New("cannot start a container that has stopped")
		case libcontainer.Running:
			return errors.New("cannot start an already running container")
		default:
			return fmt.Errorf("cannot start a container in the %s state\n", status)
		}
	},
	After:func(context *cli.Context) error {
		container, err := getContainer(context)
		if err != nil {
			return err
		}

		// define the log file
		var filename = "/var/log/docker-run.log"
		logFile, err  := os.Create(filename)
		defer logFile.Close()
		if err != nil {
			log.Fatalln("open file error !")
			return err
		}
		debugLog := log.New(logFile, "[Debug]", log.Llongfile)

		// get the id of this container, and parse
		id := container.ID()
		debugLog.Print(">>> handle ... ")
		debugLog.Println(id)

		// locate the digest of image
		debugLog.Println(">>> prepare to get digest ... ")
		imageDigest, configFileName, err := getDigest(id, debugLog)
		if err != nil {
			debugLog.Println("failed to get digest: " + err.Error())
			return err
		}

		// locate the pid of container
		debugLog.Println(">>> prepare to get mnt namespace ... ")
		processes, err:= container.Processes()
		pid := processes[0]
		mntNum, err := getMntNamespace(pid, debugLog)
		if err != nil {
			debugLog.Println("failed to get mnt namespace: " + err.Error())
			return err
		}

		// extend all
		recordStr := "\"" + id + " " + mntNum + " " + imageDigest + " " + configFileName + "\""
                // calculate its' hash value
                hs := sha1.New()
                hs.Write([]byte(recordStr))
                hashHexString := fmt.Sprintf("%x", hs.Sum(nil))
                hashString := fmt.Sprintf("%s", hs.Sum(nil))
                
                debugLog.Printf(">>> prepare to extend %s %s\n", recordStr, hashHexString)
		err = extend(11, hashString, debugLog)
		//if err != nil {
		//	debugLog.Println("failed to extend: " + err.Error())
		//	return err
		//}

		// record into ima
		debugLog.Println(">>> prepare to record into ima ... ")
		err = record(recordStr + " " + hashHexString)
		if err != nil {
			debugLog.Println("failed to record: " + err.Error())
			return err
		}
		return err
	},
}
