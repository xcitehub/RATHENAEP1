# -*- mode: ruby -*-
# vi: set ft=ruby :

#In case script issues, run this before any attempt : find . -type f -print0 | xargs -0 dos2unix
# Vagrantfile API/syntax version. Don't touch unless you know what you're doing!
VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  
#  config.vm.network "private_network", type: "dhcp"
 
  config.vm.synced_folder ".", "/vagrant", :mount_options => ["dmode=775","fmode=775"]
    
  config.vm.define :raCent65 do |box|
    box.vm.box = "puphpet/centos65-x64"
    box.vm.hostname = "raCent65"
    box.vm.network :public_network, ip: "192.168.0.30"
    #box.vm.network "forwarded_port", guest: 3142, host: 3142, auto_correct: true
    box.vm.provider "virtualbox" do |vb|
      vb.cpus = 1
      vb.memory = 1024
    end
    box.vm.provision "shell", inline: "yum upgrade -y && yum install -y perl cpan gcc gdb zlib zlib-devel make git pcre-devel mysql mysql-devel perl-DBD-MySQL"
    box.vm.provision "shell", inline: "cd /vagrant/tools && ./setup_perl.sh"
    #box.vm.provision "shell", inline: "cd /vagrant/tools && perl -I /root/.cpan/build config.pl --auto=1 --target="Conf|DB" --DBA_pwd="ragnarok" && cd ..
  end
  
  config.vm.define :raFed21 do |box|
    #box.vm.box = "jimmidyson/fedora21-atomic" #docker support
    box.vm.box = "hansode/fedora-21-server-x86_64"
    #box.vm.box = "boxcutter/fedora21"
    #box.vm.box = "box-cutter/fedora21-i386"
    box.vm.hostname = "raFed21"
    box.vm.network :public_network, ip: "192.168.0.30"
    #box.vm.network "forwarded_port", guest: 3142, host: 3142, auto_correct: true
    box.vm.provider "virtualbox" do |vb|
      vb.cpus = 1
      vb.memory = 1024
    end
    box.vm.provision "shell", inline: "yum upgrade -y && yum install -y perl cpan gcc gdb zlib zlib-devel make git mariadb-devel pcre-devel"
    box.vm.provision "shell", inline: "cd /vagrant/tools && ./setup_perl.sh"
    #box.vm.provision "shell", inline: "cd /vagrant/tools && perl -I /root/.cpan/build config.pl --auto=1 --target="Conf|DB" --DBA_pwd="ragnarok" && cd ..
  end
  
  config.vm.define :raUbun64 do |box|
    box.vm.box = "ubuntu/trusty64"
    box.vm.hostname = "raUbun64"
    box.vm.network :public_network, ip: "192.168.0.30"
    #box.vm.network "forwarded_port", guest: 3142, host: 3142, auto_correct: true
    box.vm.provider "virtualbox" do |vb|
      vb.cpus = 1
      vb.memory = 1024
    end
    box.vm.provision "shell", inline: "apt-get update && apt-get upgrade -y && apt-get install -y perl cpan gcc gdb zlib zlib-devel make git mariadb-devel pcre-devel"
    box.vm.provision "shell", inline: "cd /vagrant/tools && ./setup_perl.sh"
    #box.vm.provision "shell", inline: "cd /vagrant/tools && perl -I /root/.cpan/build config.pl --auto=1 --target="Conf|DB" --DBA_pwd="ragnarok" && cd ..
  end

  config.vm.define :raWin12 do |box|
    box.vm.box = "scorebig/windows-2012R2-SC"
    box.vm.hostname = "raWin12"
    box.vm.network :public_network, ip: "192.168.0.30"
    #box.vm.network "forwarded_port", guest: 3142, host: 3142, auto_correct: true
    box.vm.provider "virtualbox" do |vb|
      vb.cpus = 1
      vb.memory = 1024
    end
  end

  config.vm.define :raOsX10 do |box|
    box.vm.box = "jhcook/osx-yosemite-10.10"
    box.vm.hostname = "raOsX10"
    box.vm.network :public_network, ip: "192.168.0.30"
    #box.vm.network "forwarded_port", guest: 3142, host: 3142, auto_correct: true
  end

end
